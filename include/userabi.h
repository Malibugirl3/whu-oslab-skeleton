#ifndef USERABI_H
#define USERABI_H

/*
 * userabi.h
 *
 * 用户态和内核态共享的 ABI 常量。
 * 这里的内容属于“系统调用边界契约”，不是内核私有实现细节。
 */

/* syscall numbers */
#define SYS_fork   1
#define SYS_exit   2
#define SYS_wait   3
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_open   15
#define SYS_write  16
#define SYS_read   17
#define SYS_close  18
#define SYS_unlink 19
#define SYS_exec   20

/* open flags */
#define O_RDONLY 0x000
#define O_CREAT  0x001
#define O_WRONLY 0x002
#define O_RDWR   0x004

/* exec argv limits */
#define MAXARG    32
#define MAXARGLEN 32

#endif /* USERABI_H */