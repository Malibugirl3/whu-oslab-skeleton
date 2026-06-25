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
#define SYS_mkdir  21
#define SYS_chdir  22
#define SYS_fstat  23
#define SYS_dup    24
#define SYS_dup2   25
#define SYS_pipe   26
#define SYS_stat   27
#define SYS_getcwd 28

#define NELEM_SYSCALL 32

/* path limits */
#define MAXPATH 128

/* open flags */
#define O_RDONLY 0x000
#define O_CREAT  0x001
#define O_WRONLY 0x002
#define O_RDWR   0x004
#define O_TRUNC  0x008
#define O_APPEND 0x010

/* exec argv limits */
#define MAXARG    32
#define MAXARGLEN 32

#endif /* USERABI_H */