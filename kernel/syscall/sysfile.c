/* sysfile.c — 文件系统调用（Lab7 Layer 5）
 *
 * 将用户态参数"翻译"成 file.c / fs.c 的内部调用
 */

#include "defs.h"
#include "param.h"
#include "proc.h"
#include "types.h"
#include "file.h"
#include "userabi.h"

static void normalize_path(char *path) {
    char comps[32][DIRSIZ + 1];
    char tmp[MAXPATH];
    int ncomp = 0;
    int i = 0;
    int j = 0;

    if (path[0] != '/') {
        tmp[0] = '/';
        j = 1;
        while (path[i] && j < MAXPATH - 1)
            tmp[j++] = path[i++];
        tmp[j] = 0;
    } else {
        safestrcpy(tmp, path, MAXPATH);
    }

    i = 0;
    while (tmp[i]) {
        while (tmp[i] == '/')
            i++;
        if (!tmp[i])
            break;
        int start = i;
        while (tmp[i] && tmp[i] != '/')
            i++;
        int len = i - start;
        if (len == 1 && tmp[start] == '.')
            continue;
        if (len == 2 && tmp[start] == '.' && tmp[start + 1] == '.') {
            if (ncomp > 0)
                ncomp--;
            continue;
        }
        if (ncomp >= 32 || len >= DIRSIZ)
            continue;
        memmove(comps[ncomp], &tmp[start], len);
        comps[ncomp][len] = 0;
        ncomp++;
    }

    if (ncomp == 0) {
        path[0] = '/';
        path[1] = 0;
        return;
    }

    j = 0;
    for (i = 0; i < ncomp; i++) {
        path[j++] = '/';
        int k = 0;
        while (comps[i][k])
            path[j++] = comps[i][k++];
    }
    path[j] = 0;
}

static void make_abs_path(struct proc *p, const char *path, char *abs) {
    int i = 0;

    if (path[0] == '/') {
        safestrcpy(abs, path, MAXPATH);
    } else {
        if (p->cwdpath[0] == '/' && p->cwdpath[1] == 0) {
            abs[i++] = '/';
        } else {
            safestrcpy(abs, p->cwdpath, MAXPATH);
            i = strlen(abs);
            if (i > 0 && abs[i - 1] != '/' && i < MAXPATH - 1)
                abs[i++] = '/';
        }
        int j = 0;
        while (path[j] && i < MAXPATH - 1)
            abs[i++] = path[j++];
        abs[i] = 0;
    }
    normalize_path(abs);
}

static int fdalloc(struct file *f) {
    struct proc *p = myproc();

    for (int fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd] == 0) {
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}


static struct inode*
create(char *path, short type)
{
    struct inode *ip, *dp;
    char name[DIRSIZ];

    if ((dp = nameiparent(path, name)) == 0) 
        return 0;

    ilock(dp);

    if ((ip = dirlookup(dp, name, 0)) != 0) {
        // 文件已存在, 返回
        iunlock(dp);
        iput(dp);
        ilock(ip);

        if (type == T_FILE && ip->type == T_FILE)
            return ip;

        // TODO : 这里直接返回了要么在上层处理返回 0 的情况，要么在这里判断是否是文件夹
        iunlock(ip);
        iput(ip);
        return 0;
    } 

    if ((ip = ialloc(dp->dev, type)) == 0) {
        iunlock(dp);
        iput(dp);
        return 0;
    }

    ilock(ip);
    ip->nlink = 1;
    iupdate(ip);

    if (type == T_DIR) {
        dp->nlink++;
        iupdate(dp);

        if (dirlink(ip, ".", ip->inum) < 0 ||
            dirlink(ip, "..", dp->inum) < 0)
            panic("create dots");
    }

    if (dirlink(dp, name, ip->inum) < 0) {
        if (type == T_DIR) 
            dp->nlink--;

        ip->nlink = 0;
        iupdate(ip);
        iunlock(ip);
        iput(ip);
        iunlock(dp);
        iput(dp);
        return 0;
    }

    iunlock(dp);
    iput(dp);

    return ip;
}

/* ================================================================
 * sys_open — 打开（或创建）文件，返回 fd
 * ================================================================ */
uint64
sys_open(void)
{
    char path[MAXPATH];
    int fd, flags;
    struct file *f;
    struct inode *ip;

    argint(1, &flags);                    // 拿 flag
    // argstr(0, path, sizeof(path));        // 拿路径
    if (argstr(0, path, sizeof(path)) < 0) {
        return -1;
    } 

    if (flags & O_CREAT) {
        begin_op();

        if ((ip = create(path, T_FILE)) == 0) {
            end_op();
            return -1;
        }

    } else {
        if ((ip = namei(path)) == 0)
            return -1;

        ilock(ip);

        if (ip->type == T_DIR && (flags & (O_WRONLY | O_RDWR))) {
            iunlock(ip);
            iput(ip);
            return -1;
        }

        if ((flags & O_TRUNC) && ip->type == T_FILE &&
            (flags & (O_WRONLY | O_RDWR))) {
            begin_op();
            itrunc(ip);
            ip->size = 0;
            iupdate(ip);
            end_op();
        }
    }

    // 3. 分配 file 结构
    if ((f = filealloc()) == 0) {
        iunlock(ip);
        iput(ip);
        if (flags & O_CREAT)
            end_op();
        return -1;
    }
    f->type = FD_INODE;
    f->ip = ip;
    if (flags & O_APPEND)
        f->off = ip->size;
    else
        f->off = 0;
    f->readable = !(flags & O_WRONLY);
    f->writable = (flags & O_WRONLY) || (flags & O_RDWR);

    // 4. 分配 fd
    for (fd = 2; fd < NOFILE; fd++) {
        if (myproc()->ofile[fd] == 0) {
            myproc()->ofile[fd] = f;
            break;
        }
    }
    if (fd == NOFILE) {
        iunlock(ip);
        fileclose(f);
        if (flags & O_CREAT)
            end_op();
        return -1;
    }

    iunlock(ip);
    if (flags & O_CREAT)
        end_op();
    return fd;
}

uint64
sys_mkdir(void) 
{
    char path[MAXPATH];
    struct inode *ip;

    if (argstr(0, path, sizeof(path)) < 0) 
        return -1;

    begin_op();

    if ((ip = create(path, T_DIR)) == 0) {
        end_op();
        return -1;
    }

    iunlock(ip);
    iput(ip);

    end_op();
    return 0;
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

    // fd=0 (stdin) 优先使用 dup 后的 ofile，否则 UART
    if (fd == 0) {
        f = myproc()->ofile[0];
        if (f)
            return fileread(f, addr, n);

        int i = 0;
        char c;
        char kbuf[128];
        int max = n;

        if (max > (int)sizeof(kbuf))
            max = sizeof(kbuf);

        while (i < max) {
            c = (char)uart_getc();

            // Delete/方向键等通常会发送 ESC 开头的控制序列。
            // 它们不应该进入命令缓冲区，否则屏幕显示和实际解析会不一致。
            if (c == 0x1b) {
                do {
                    c = (char)uart_getc();
                } while (!((c >= '@' && c <= '~') || c == '\n' || c == '\r'));
                continue;
            }

            if (c == '\b' || c == 0x7f) {
                if (i > 0) {
                    i--;
                    uart_putc('\b');
                    uart_putc(' ');
                    uart_putc('\b');
                }
                continue;
            }

            if (c == '\r')
                c = '\n';

            if (c != '\n' && (c < ' ' || c == 0x7f))
                continue;

            // 简单回显，让终端看起来像真正命令行。
            uart_putc(c);

            kbuf[i] = c;
            i++;

            if (c == '\n')
                break;
        }

        if (copyout(myproc()->pagetable, addr, kbuf, i) < 0)
            return -1;
        return i;
    }

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

    // fd=1 (stdout) 优先使用 dup 后的 ofile，否则 UART
    if (fd == 1) {
        f = myproc()->ofile[1];
        if (f) {
            begin_op();
            int r = filewrite(f, addr, n);
            end_op();
            return r;
        }

        for (int i = 0; i < n; i++) {
            char c;
            copyin(myproc()->pagetable, &c, addr + i, 1);
            uart_putc(c);
        }
        return n;
    }

    f = myproc()->ofile[fd];
    if (f == 0)
        return -1;

    begin_op();
    int r = filewrite(f, addr, n);
    end_op();
    return r;
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
    begin_op();
    fileclose(f);
    end_op();
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

    begin_op();

    /* 步骤2: 解析出父目录 + 文件名 */
    dp = nameiparent(path, name);
    if (dp == 0) {
        end_op();
        return -1;
    }

    if ((name[0] == '.' && name[1] == 0) ||
        (name[0] == '.' && name[1] == '.' && name[2] == 0)) {
        iput(dp);
        end_op();
        return -1;
    }

    /* 步骤3: 锁父目录，查找目标文件（同时拿到 dirent 偏移）*/
    ilock(dp);
    ip = dirlookup(dp, name, &off);
    if (ip == 0) {
        iunlock(dp);
        iput(dp);
        end_op();
        return -1;   // 文件不存在
    }

    ilock(ip);

    /* 步骤4: 不允许删除目录（简化处理）*/
    if (ip->type == T_DIR) {
        iunlock(ip);
        iput(ip);
        iunlock(dp);
        iput(dp);
        end_op();
        return -1;
    }

    /* 步骤5: 清除目录项 — 把 dirent 的 inum 设为 0 然后写回 */
    memset(&de, 0, sizeof(de));
    if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de)) {
        iunlock(ip);
        iput(ip);
        iunlock(dp);
        iput(dp);
        end_op();
        return -1;
    }

    /* 步骤6: 释放父目录 */
    iunlock(dp);
    iput(dp);

    /* 步骤7: 减少链接计数 */
    ip->nlink--;
    iupdate(ip);

    /* 步骤8: 释放文件 inode
     *       此时如果没人打开了 → ref==0, nlink==0 → iput 内部触发 itrunc */
    iunlock(ip);
    iput(ip);

    end_op();
    return 0;
}


uint64
sys_chdir(void)
{
    char path[MAXPATH];
    struct inode *ip;
    struct proc *p = myproc();

    if (argstr(0, path, sizeof(path)) < 0)
        return -1;

    if ((ip = namei(path)) == 0)
        return -1;

    ilock(ip);
    
    if (ip->type != T_DIR) {
        iunlock(ip);
        iput(ip);
        return -1;
    }

    iunlock(ip);

    iput(p->cwd);
    p->cwd = ip;
    make_abs_path(p, path, p->cwdpath);

    return 0;

}

uint64
sys_getcwd(void)
{
    uint64 addr;
    struct proc *p = myproc();

    argaddr(0, &addr);
    if (copyout(p->pagetable, addr, p->cwdpath, strlen(p->cwdpath) + 1) < 0)
        return -1;
    return 0;
}

uint64
sys_stat(void)
{
    char path[MAXPATH];
    uint64 addr;
    struct inode *ip;
    struct stat st;

    if (argstr(0, path, sizeof(path)) < 0)
        return -1;
    argaddr(1, &addr);

    if ((ip = namei(path)) == 0)
        return -1;

    ilock(ip);
    stati(ip, &st);
    iunlock(ip);
    iput(ip);

    if (copyout(myproc()->pagetable, addr, (char *)&st, sizeof(st)) < 0)
        return -1;
    return 0;
}

uint64
sys_dup(void)
{
    int fd;
    struct file *f;

    argint(0, &fd);
    if (fd < 0 || fd >= NOFILE)
        return -1;

    f = myproc()->ofile[fd];
    if (f == 0)
        return -1;

    return fdalloc(filedup(f));
}

uint64
sys_dup2(void)
{
    int oldfd, newfd;
    struct file *f;
    struct proc *p = myproc();

    argint(0, &oldfd);
    argint(1, &newfd);
    if (oldfd < 0 || oldfd >= NOFILE || newfd < 0 || newfd >= NOFILE)
        return -1;
    if (oldfd == newfd)
        return newfd;

    f = p->ofile[oldfd];
    if (f == 0)
        return -1;

    if (p->ofile[newfd])
        fileclose(p->ofile[newfd]);
    p->ofile[newfd] = filedup(f);
    return newfd;
}

uint64
sys_pipe(void)
{
    uint64 fdarray;
    int fds[2];

    argaddr(0, &fdarray);
    if (createpipe(fds) < 0)
        return -1;
    if (copyout(myproc()->pagetable, fdarray, (char *)fds, sizeof(fds)) < 0) {
        struct proc *p = myproc();
        struct file *f0 = p->ofile[fds[0]];
        struct file *f1 = p->ofile[fds[1]];
        p->ofile[fds[0]] = 0;
        p->ofile[fds[1]] = 0;
        fileclose(f0);
        fileclose(f1);
        return -1;
    }
    return 0;
}

uint64
sys_fstat(void)
{
    int fd;
    uint64 addr;
    struct file *f;

    argint(0, &fd);
    argaddr(1, &addr);

    if (fd < 0 || fd >= NOFILE)
        return -1;

    f = myproc()->ofile[fd];
    if (f == 0)
        return -1;

    return filestat(f, addr);
}