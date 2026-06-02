#ifndef SYSCALL_NR_H
#define SYSCALL_NR_H

#define SYS_fork 1
#define SYS_exit 2
#define SYS_wait 3
#define SYS_getpid 11
#define SYS_sbrk 12
#define SYS_open  15
#define SYS_write 16
#define SYS_read  17
#define SYS_close 18
#define SYS_unlink 19

/* open 系统调用的 flags */
#define O_RDONLY 0x000
#define O_CREAT  0x001
#define O_WRONLY 0x002
#define O_RDWR   0x004

#endif /* SYSCALL_NR_H */
