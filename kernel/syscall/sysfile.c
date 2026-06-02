/* sysfile.c — 文件系统调用（Lab7 Layer 5）
 *
 * 将用户态参数"翻译"成 file.c / fs.c 的内部调用
 */

#include "defs.h"
#include "param.h"
#include "proc.h"
#include "types.h"
#include "file.h"
#include "syscall_nr.h"


/* ================================================================
 * sys_open — 打开（或创建）文件，返回 fd
 * ================================================================ */
uint64
sys_open(void)
{
    char path[128];
    int fd, flags;
    struct file *f;
    struct inode *ip, *dp;
    char name[DIRSIZ];

    argint(1, &flags);                    // 拿 flag
    argstr(0, path, sizeof(path));        // 拿路径

    // 1. 获取父目录 + 文件名
    if ((dp = nameiparent(path, name)) == 0)
        return -1;

    ilock(dp);

    // 2. 查找文件是否已存在
    if ((ip = dirlookup(dp, name, 0)) != 0) {
        // 已存在 → 直接打开
        iunlock(dp);
        iput(dp);
        ilock(ip);
        if (ip->type == T_DIR && (flags & O_WRONLY)) {
            iunlock(ip);
            iput(ip);
            return -1;   // 不能只写一个目录
        }
    } else {
        // 不存在 → 需要创建？
        if (!(flags & O_CREAT)) {
            iunlock(dp);
            iput(dp);
            return -1;
        }
        // 创建新文件
        if ((ip = ialloc(dp->dev, T_FILE)) == 0) {
            iunlock(dp);
            iput(dp);
            return -1;
        }
        ilock(ip);
        ip->nlink = 1;   // 新文件至少有一个目录项指向它
        if (dirlink(dp, name, ip->inum) < 0) {
            iunlock(ip);
            iput(ip);
            iunlock(dp);
            iput(dp);
            return -1;
        }
        iunlock(dp);
        iput(dp);
    }

    // 3. 分配 file 结构
    if ((f = filealloc()) == 0) {
        iunlock(ip);
        iput(ip);
        return -1;
    }
    f->type = FD_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(flags & O_WRONLY);
    f->writable = (flags & O_WRONLY) || (flags & O_RDWR);

    // 4. 分配 fd
    for (fd = 0; fd < NOFILE; fd++) {
        if (myproc()->ofile[fd] == 0) {
            myproc()->ofile[fd] = f;
            break;
        }
    }
    if (fd == NOFILE) {
        fileclose(f);
        return -1;
    }

    iunlock(ip);
    return fd;
}

/* ================================================================
 * sys_read — 从文件读数据
 * ================================================================ */
uint64
sys_read(void)
{
    int fd, n;
    uint64 addr;
    struct file *f;

    argint(0, &fd);
    argaddr(1, &addr);
    argint(2, &n);

    if (fd < 0 || fd >= NOFILE)
        return -1;
    f = myproc()->ofile[fd];
    if (f == 0)
        return -1;

    return fileread(f, addr, n);
}

/* ================================================================
 * sys_write — 向文件写数据（fd=1 重定向到 UART）
 * ================================================================ */
uint64
sys_write(void)
{
    int fd, n;
    uint64 addr;
    struct file *f;

    argint(0, &fd);
    argaddr(1, &addr);
    argint(2, &n);

    if (fd < 0 || fd >= NOFILE)
        return -1;

    // fd=1 (stdout) 直接输出到 UART
    if (fd == 1) {
        char buf[256];
        int m = n;
        if (m > 256) m = 256;
        if (copyin(myproc()->pagetable, buf, addr, m) < 0)
            return -1;
        for (int i = 0; i < m; i++)
            uart_putc(buf[i]);
        return m;
    }

    f = myproc()->ofile[fd];
    if (f == 0)
        return -1;

    return filewrite(f, addr, n);
}

/* ================================================================
 * sys_close — 关闭文件描述符
 * ================================================================ */
uint64
sys_close(void)
{
    int fd;
    struct file *f;

    argint(0, &fd);
    if (fd < 0 || fd >= NOFILE)
        return -1;

    f = myproc()->ofile[fd];
    if (f == 0)
        return -1;

    myproc()->ofile[fd] = 0;
    fileclose(f);
    return 0;
}

/* ================================================================
 * sys_unlink — 删除文件
 *
 * 连接关系：
 *   用户态 unlink(path) → syscall 分发 → sys_unlink()
 *   → 调用 Layer 3 的 nameiparent / dirlookup / writei / iput
 *
 * 流程：
 *   1. argstr → 拿到路径字符串
 *   2. nameiparent → 解析出父目录 dp + 文件名 name
 *   3. ilock(dp) + dirlookup(dp, name, &off) → 找到文件 + 偏移
 *   4. 校验：文件必须存在且不是目录
 *   5. writei(dp, 0, &de, off, ...) → 把 dirent 的 inum 清零
 *   6. iput(dp) → 释放父目录
 *   7. ip->nlink-- → 减少链接计数
 *   8. iput(ip) → ref==0 且 nlink==0 时触发 itrunc 回收资源
 * ================================================================ */
uint64
sys_unlink(void)
{
    char path[128], name[DIRSIZ];
    struct inode *ip, *dp;
    struct dirent de;
    uint off;

    /* 步骤1: 从用户空间拿路径 */
    if (argstr(0, path, sizeof(path)) < 0)
        return -1;

    /* 步骤2: 解析出父目录 + 文件名 */
    dp = nameiparent(path, name);
    if (dp == 0)
        return -1;

    /* 步骤3: 锁父目录，查找目标文件（同时拿到 dirent 偏移）*/
    ilock(dp);
    ip = dirlookup(dp, name, &off);
    if (ip == 0) {
        iunlock(dp);
        iput(dp);
        return -1;   // 文件不存在
    }

    /* 步骤4: 不允许删除目录（简化处理）*/
    if (ip->type == T_DIR) {
        iunlock(ip);
        iput(ip);
        iunlock(dp);
        iput(dp);
        return -1;
    }

    /* 步骤5: 清除目录项 — 把 dirent 的 inum 设为 0 然后写回 */
    memset(&de, 0, sizeof(de));
    de.inum = 0;
    writei(dp, 0, (uint64)&de, off, sizeof(de));

    /* 步骤6: 释放父目录 */
    iunlock(dp);
    iput(dp);

    /* 步骤7: 减少链接计数 */
    ip->nlink--;

    /* 步骤8: 释放文件 inode
     *       此时如果没人打开了 → ref==0, nlink==0 → iput 内部触发 itrunc */
    iunlock(ip);
    iput(ip);

    return 0;
}
