/* mkfs.c — 最小化文件系统镜像创建工具
 *
 * 使用方式：编译后运行 ./mkfs fs.img
 * 生成一个包含合法 superblock + 根目录 inode 的磁盘镜像。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define BSIZE       1024
#define FSMAGIC     0x10203040
#define ROOTINO     1
#define NDIRECT     12
#define DIRSIZ      14
#define T_DIR       2
#define T_FILE      1
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

unsigned char buf[BSIZE];

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: mkfs <output>\n");
        return 1;
    }

    int fd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return 1; }

    /* 写入 FSSIZE 个零块 */
    memset(buf, 0, BSIZE);
    for (int i = 0; i < FSSIZE; i++)
        write(fd, buf, BSIZE);

    /* ---- Block 1: superblock ---- */
    lseek(fd, BSIZE, SEEK_SET);
    struct superblock sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic      = FSMAGIC;
    sb.size       = FSSIZE;
    sb.nblocks    = FSSIZE;
    sb.ninodes    = NINODES;
    sb.nlog       = NLOGBLOCKS;
    sb.logstart   = 2;
    sb.inodestart = 2 + NLOGBLOCKS + NBITMAP;  /* 2 + 30 + 1 = 33 */
    sb.bmapstart  = 2 + NLOGBLOCKS;            /* 2 + 30 = 32 */
    memcpy(buf, &sb, sizeof(sb));
    lseek(fd, BSIZE, SEEK_SET);
    write(fd, buf, BSIZE);

    /* ---- root inode (inum=1, Block sb.inodestart) ---- */
    struct dinode root;
    memset(&root, 0, sizeof(root));
    root.type  = T_DIR;
    root.nlink = 1;
    root.size  = 2 * sizeof(struct dirent);   /* "." and ".." */

    memset(buf, 0, BSIZE);
    /* ROOTINO=1, 放在第 1 个 dinode 槽位（偏移 sizeof(dinode)）*/
    memcpy(buf + sizeof(struct dinode), &root, sizeof(root));
    lseek(fd, sb.inodestart * BSIZE, SEEK_SET);
    write(fd, buf, BSIZE);

    /* ---- root directory data (Block 46) ---- */
    /* mark all metadata blocks (0..inode_end) as busy in bitmap */
    int meta_end = sb.inodestart + (sb.ninodes + IPB - 1) / IPB;
    for (int blk = 0; blk <= meta_end; blk++) {
        lseek(fd, sb.bmapstart * BSIZE + blk / 8, SEEK_SET);
        read(fd, buf, 1);
        buf[0] |= (1 << (blk % 8));
        lseek(fd, sb.bmapstart * BSIZE + blk / 8, SEEK_SET);
        write(fd, buf, 1);
    }

    /* assign first data block at meta_end + 1 for root directory */
    int root_block = meta_end + 1;

    /* update root inode's addrs[0] (inum=1, 偏移 sizeof(dinode)) */
    root.addrs[0] = root_block;
    lseek(fd, sb.inodestart * BSIZE + sizeof(struct dinode), SEEK_SET);
    write(fd, &root, sizeof(root));

    /* write "." and ".." entries */
    struct dirent de;
    memset(&de, 0, sizeof(de));
    de.inum = 1;
    strncpy(de.name, ".", DIRSIZ);
    memset(buf, 0, BSIZE);
    memcpy(buf, &de, sizeof(de));
    memcpy(buf + sizeof(de), &de, sizeof(de));  /* ".." also points to root */
    strncpy(((struct dirent*)(buf + sizeof(de)))->name, "..", DIRSIZ);
    lseek(fd, root_block * BSIZE, SEEK_SET);
    write(fd, buf, BSIZE);

    close(fd);
    printf("mkfs: created %s (%d blocks)\n", argv[1], FSSIZE);
    return 0;
}
