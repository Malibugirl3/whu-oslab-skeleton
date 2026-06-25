/* file.c — 文件描述符层（Lab7 Layer 4） */

#include "defs.h"
#include "param.h"
#include "proc.h"
#include "types.h"
#include "file.h"

struct {
    struct file file[NFILE];
} ftable;

struct file *
filealloc(void)
{
    for (int i = 0; i < NFILE; i++) {
        if (ftable.file[i].type == FD_NONE) {
            ftable.file[i].type = FD_INODE;
            ftable.file[i].ref = 1;
            ftable.file[i].readable = 0;
            ftable.file[i].writable = 0;
            ftable.file[i].pipe = 0;
            ftable.file[i].ip = 0;
            ftable.file[i].off = 0;
            return &ftable.file[i];
        }
    }
    return 0;
}

struct file *
filedup(struct file *f)
{
    if (f->type == FD_NONE)
        panic("filedup");
    f->ref++;
    return f;
}

void
fileclose(struct file *f)
{
    if (f->type == FD_NONE)
        return;

    f->ref--;
    if (f->ref > 0)
        return;

    if (f->type == FD_PIPE) {
        pipefileclose(f);
    } else if (f->type == FD_INODE) {
        if (f->ip) {
            iput(f->ip);
            f->ip = 0;
        }
    }
    f->type = FD_NONE;
    f->readable = 0;
    f->writable = 0;
    f->pipe = 0;
    f->off = 0;
}

int
fileread(struct file *f, uint64 addr, int n)
{
    int r = 0;

    if (!f->readable)
        return -1;

    if (f->type == FD_PIPE)
        return piperead(f, addr, n);

    ilock(f->ip);
    r = readi(f->ip, 1, addr, f->off, n);
    if (r > 0)
        f->off += r;
    iunlock(f->ip);

    return r;
}

int
filewrite(struct file *f, uint64 addr, int n)
{
    int r = 0;

    if (!f->writable)
        return -1;

    if (f->type == FD_PIPE)
        return pipewrite(f, addr, n);

    ilock(f->ip);
    r = writei(f->ip, 1, addr, f->off, n);
    if (r > 0)
        f->off += r;
    iunlock(f->ip);

    return r;
}

int
filestat(struct file *f, uint64 addr)
{
    struct stat st;

    if (f->type != FD_INODE)
        return -1;

    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);

    if (copyout(myproc()->pagetable, addr, (char *)&st, sizeof(st)) < 0)
        return -1;

    return 0;
}
