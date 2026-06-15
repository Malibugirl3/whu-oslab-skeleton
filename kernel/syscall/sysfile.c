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

    // fd=0 (stdin) 从 UART 读取输入。
    if (fd == 0) {
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
