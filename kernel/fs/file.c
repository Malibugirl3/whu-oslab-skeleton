/* file.c — 文件描述符层（Lab7 Layer 4）
 *
 * 管理"打开的文件"状态：fd → struct file → inode 的映射
 */

#include "defs.h"
#include "param.h"
#include "proc.h"
#include "types.h"
#include "file.h"

/* 全局文件表 */
struct {
    struct file file[NFILE];
} ftable;

/* 初始化 file 表的锁（单核可省略）*/
// void ftable_init(void) {
//     for (int i = 0; i < NFILE; i++)
//         ftable.file[i].type = FD_NONE;
// }

/* ================================================================
 * filealloc — 从全局表中分配一个空 file 槽位
 * ================================================================ */
struct file*
filealloc(void)
{
    for (int i = 0; i < NFILE; i++) {
        if (ftable.file[i].type == FD_NONE) {
            ftable.file[i].type = FD_INODE;
            ftable.file[i].ref = 1;
            ftable.file[i].readable = 0;
            ftable.file[i].writable = 0;
            ftable.file[i].ip = 0;
            ftable.file[i].off = 0;
            return &ftable.file[i];
        }
    }
    return 0;
}

/* ================================================================
 * filedup — fork 时复制 fd（只是 ref++，不复制 file 本身）
 * ================================================================ */
struct file*
filedup(struct file *f)
{
    if (f->type != FD_INODE)
        panic("filedup: file type error");
    f->ref++;
    return f;
}

/* ================================================================
 * fileclose — 关闭文件（ref--，最后一人负责释放 inode）
 * ================================================================ */
void
fileclose(struct file *f)
{
    if (f->type != FD_INODE)
        return;

    f->ref--;
    if (f->ref > 0)
        return;

    // 最后一个人
    if (f->ip) {
        iput(f->ip);
        f->ip = 0;
    }
    f->type = FD_NONE;
    f->readable = 0;
    f->writable = 0;
    f->off = 0;
}

/* ================================================================
 * fileread — 从文件读数据（检查权限 → readi → 更新 off）
 * ================================================================ */
int
fileread(struct file *f, uint64 addr, int n)
{
    int r = 0;

    if (!f->readable)
        return -1;

    ilock(f->ip);
    r = readi(f->ip, 1, addr, f->off, n);
    if (r > 0)
        f->off += r;
    iunlock(f->ip);

    return r;
}

/* ================================================================
 * filewrite — 向文件写数据（检查权限 → writei → 更新 off）
 * ================================================================ */
int
filewrite(struct file *f, uint64 addr, int n)
{
    int r = 0;

    if (!f->writable)
        return -1;

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