/* mkfs.c - build the root file system image.
 *
 * Usage:
 *   ./mkfs fs.img user/_sh user/_fstest
 *
 * Files whose basename starts with '_' are installed without that prefix.
 * For example, user/_sh becomes /sh inside the file system.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BSIZE       1024
#define FSMAGIC     0x10203040
#define ROOTINO     1
#define NDIRECT     12
#define NINDIRECT   (BSIZE / sizeof(unsigned int))
#define MAXFILE     (NDIRECT + NINDIRECT)
#define DIRSIZ      14
#define T_FILE      1
#define T_DIR       2
#define FSSIZE      1000
#define NINODES     200
#define NINODEBLOCKS 13
#define NLOGBLOCKS  30
#define NBITMAP     1
#define IPB         (BSIZE / sizeof(struct dinode))

struct dinode {
    short type;
    short major;
    short minor;
    short nlink;
    unsigned int size;
    unsigned int addrs[NDIRECT + 1];
};

struct superblock {
    unsigned int magic;
    unsigned int size;
    unsigned int nblocks;
    unsigned int ninodes;
    unsigned int nlog;
    unsigned int logstart;
    unsigned int inodestart;
    unsigned int bmapstart;
};

struct dirent {
    unsigned short inum;
    char name[DIRSIZ];
};

static int fsfd;
static unsigned int freeinode = ROOTINO;
static unsigned int freeblock;
static struct superblock sb;
static unsigned char zeroes[BSIZE];

static void
die(const char *msg)
{
    perror(msg);
    exit(1);
}

static void
wsect(unsigned int sec, void *buf)
{
    if (lseek(fsfd, sec * BSIZE, SEEK_SET) != sec * BSIZE)
        die("lseek");
    if (write(fsfd, buf, BSIZE) != BSIZE)
        die("write sector");
}

static void
rsect(unsigned int sec, void *buf)
{
    if (lseek(fsfd, sec * BSIZE, SEEK_SET) != sec * BSIZE)
        die("lseek");
    if (read(fsfd, buf, BSIZE) != BSIZE)
        die("read sector");
}

static void
winode(unsigned int inum, struct dinode *ip)
{
    unsigned char buf[BSIZE];
    unsigned int bn = sb.inodestart + inum / IPB;
    struct dinode *dip;

    rsect(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *dip = *ip;
    wsect(bn, buf);
}

static void
rinode(unsigned int inum, struct dinode *ip)
{
    unsigned char buf[BSIZE];
    unsigned int bn = sb.inodestart + inum / IPB;
    struct dinode *dip;

    rsect(bn, buf);
    dip = ((struct dinode *)buf) + (inum % IPB);
    *ip = *dip;
}

static unsigned int
ialloc(short type)
{
    unsigned int inum = freeinode++;
    struct dinode din;

    if (inum >= NINODES) {
        fprintf(stderr, "mkfs: out of inodes\n");
        exit(1);
    }

    memset(&din, 0, sizeof(din));
    din.type = type;
    din.nlink = 1;
    winode(inum, &din);
    return inum;
}

static unsigned int
balloc(void)
{
    if (freeblock >= FSSIZE) {
        fprintf(stderr, "mkfs: out of blocks\n");
        exit(1);
    }
    return freeblock++;
}

static void
iappend(unsigned int inum, void *xp, int n)
{
    char *p = xp;
    unsigned int fbn, off, n1;
    struct dinode din;
    unsigned char buf[BSIZE];
    unsigned int indirect[NINDIRECT];

    rinode(inum, &din);
    off = din.size;

    while (n > 0) {
        fbn = off / BSIZE;
        if (fbn >= MAXFILE) {
            fprintf(stderr, "mkfs: file too large\n");
            exit(1);
        }

        unsigned int x;
        if (fbn < NDIRECT) {
            if (din.addrs[fbn] == 0)
                din.addrs[fbn] = balloc();
            x = din.addrs[fbn];
        } else {
            if (din.addrs[NDIRECT] == 0)
                din.addrs[NDIRECT] = balloc();

            rsect(din.addrs[NDIRECT], (char *)indirect);
            if (indirect[fbn - NDIRECT] == 0) {
                indirect[fbn - NDIRECT] = balloc();
                wsect(din.addrs[NDIRECT], (char *)indirect);
            }
            x = indirect[fbn - NDIRECT];
        }

        n1 = BSIZE - (off % BSIZE);
        if (n1 > (unsigned int)n)
            n1 = n;

        rsect(x, buf);
        memcpy(buf + (off % BSIZE), p, n1);
        wsect(x, buf);

        n -= n1;
        off += n1;
        p += n1;
    }

    din.size = off;
    winode(inum, &din);
}

static void
write_bitmap(void)
{
    unsigned char buf[BSIZE];

    memset(buf, 0, sizeof(buf));
    for (unsigned int i = 0; i < freeblock; i++)
        buf[i / 8] |= 1 << (i % 8);

    wsect(sb.bmapstart, buf);
}

static char *
fsname(char *path)
{
    char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    if (name[0] == '_')
        name++;
    return name;
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: mkfs <output> [files...]\n");
        return 1;
    }

    fsfd = open(argv[1], O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fsfd < 0)
        die("open fs.img");

    memset(zeroes, 0, sizeof(zeroes));
    for (int i = 0; i < FSSIZE; i++)
        wsect(i, zeroes);

    memset(&sb, 0, sizeof(sb));
    sb.magic      = FSMAGIC;
    sb.size       = FSSIZE;
    sb.nblocks    = FSSIZE;
    sb.ninodes    = NINODES;
    sb.nlog       = NLOGBLOCKS;
    sb.logstart   = 2;
    sb.bmapstart  = 2 + NLOGBLOCKS;
    sb.inodestart = sb.bmapstart + NBITMAP;

    unsigned char sbuf[BSIZE];
    memset(sbuf, 0, sizeof(sbuf));
    memcpy(sbuf, &sb, sizeof(sb));
    wsect(1, sbuf);

    freeblock = sb.inodestart + NINODEBLOCKS;

    unsigned int rootino = ialloc(T_DIR);
    if (rootino != ROOTINO) {
        fprintf(stderr, "mkfs: root inode must be %d\n", ROOTINO);
        exit(1);
    }

    struct dirent de;
    memset(&de, 0, sizeof(de));
    de.inum = ROOTINO;
    strncpy(de.name, ".", DIRSIZ);
    iappend(ROOTINO, &de, sizeof(de));

    memset(&de, 0, sizeof(de));
    de.inum = ROOTINO;
    strncpy(de.name, "..", DIRSIZ);
    iappend(ROOTINO, &de, sizeof(de));

    for (int i = 2; i < argc; i++) {
        char *name = fsname(argv[i]);
        if (strlen(name) >= DIRSIZ) {
            fprintf(stderr, "mkfs: file name too long: %s\n", name);
            exit(1);
        }

        int fd = open(argv[i], O_RDONLY);
        if (fd < 0)
            die(argv[i]);

        unsigned int inum = ialloc(T_FILE);
        memset(&de, 0, sizeof(de));
        de.inum = inum;
        strncpy(de.name, name, DIRSIZ);
        iappend(ROOTINO, &de, sizeof(de));

        unsigned char buf[BSIZE];
        int n;
        while ((n = read(fd, buf, sizeof(buf))) > 0)
            iappend(inum, buf, n);
        if (n < 0)
            die("read input file");

        close(fd);
        printf("mkfs: added /%s\n", name);
    }

    write_bitmap();
    close(fsfd);
    printf("mkfs: created %s (%d blocks, used %u blocks)\n",
           argv[1], FSSIZE, freeblock);
    return 0;
}
