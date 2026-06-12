/* defs.h — 内核函数声明汇总
 *
 * 每完成一个实验，将新实现的函数签名添加到对应区域。
 * 这是整个内核各模块之间"相互认识"的汇总名册。
 */
#ifndef DEFS_H
#define DEFS_H

#include "types.h"

struct cpu;
struct proc;
struct context;
struct trapframe;
struct buf;
struct inode;
struct dirent;
struct sleeplock;
struct spinlock;
/* ======================================================
 * 通用工具函数
 * 文件：kernel/lib/string.c
 * ====================================================== */
void*  memset(void *dst, int c, uint64 n);
int    memcmp(const void *v1, const void *v2, uint64 n);
void*  memmove(void *dst, const void *src, uint64 n);
void*  memcpy(void *dst, const void *src, uint64 n);
int    strlen(const char *s);
int    strncmp(const char *p, const char *q, uint64 n);
char*  strncpy(char *s, const char *t, int n);
char*  safestrcpy(char *s, const char *t, int n);

/* ======================================================
 * Lab1 新增：uart 串口驱动
 * 文件：kernel/driver/uart.c
 * ====================================================== */
void uartinit(void);
void uart_putc(char c);
void uart_puts(char *s);

/* ======================================================
 * Lab2 新增：内核 printf
 * 文件：kernel/driver/console.c
 * ====================================================== */
void printf(char *fmt, ...);
void clear_screen(void);
void panic(char *msg) __attribute__((noreturn));

/* ======================================================
 * Lab3 新增：物理内存分配器
 * 文件：kernel/mm/kalloc.c
 * ====================================================== */
void kinit(void);
void *kalloc(void);
void kfree(void *pa);

/* ======================================================
 * Lab3 新增：虚拟内存 / 页表
 * 文件：kernel/mm/vm.c
 * ====================================================== */
void kvmininit(void);
void kvminithart(void);
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
int mappages(pagetable_t pagetable, uint64 pa, uint64 va, uint64 size,
             int perm);
uint64 walkaddr(pagetable_t pagetable, uint64 va);
extern pagetable_t kernel_pagetable;

/* ======================================================
 * Lab6 新增：用户虚拟内存
 * 文件：kernel/mm/vm.c
 * ====================================================== */
uint64 walkaddr(pagetable_t pagetable, uint64 va);
pagetable_t uvmcreate(void);
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz);
void uvmfree(pagetable_t pagetable, uint64 sz);

/* ======================================================
 * Lab4 新增：启动初始化
 * 文件：kernel/boot/start.c
 * ====================================================== */
void start(void);
void start_main(void);
void timerinit(void);

/* ======================================================
 * Lab4 新增：中断 / 陷阱处理
 * 文件：kernel/trap/trap.c
 * ====================================================== */
void trapinithart(void);
void plicinit(void);
void kerneltrap(void);
void usertrap(void);
void usertrapret(void);

/* ======================================================
 * Lab5 新增：进程管理
 * 文件：kernel/proc/proc.c
 * ====================================================== */
void procinit(void);
void userinit(void);
struct proc *myproc(void);
struct cpu *mycpu(void);
int allocpid(void);
struct proc *allocproc(void);
void scheduler(void) __attribute__((noreturn));
void yield(void);
void sched(void);
void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);

/* ======================================================
 * Lab5 新增：上下文切换汇编
 * 文件：kernel/proc/swtch.S
 * ====================================================== */
void swtch(struct context *old, struct context *new);

/* ======================================================
 * Lab6 新增：系统调用分发
 * 文件：kernel/syscall/syscall.c
 * ====================================================== */
void syscall(void);
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len);
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len);
int copyinstr(pagetable_t pagetable, char *dstva, uint64 srcva, uint64 max);

/* ======================================================
 * Lab6 新增：系统调用具体实现
 * 文件：kernel/syscall/sysproc.c
 * ====================================================== */
uint64 sys_getpid(void);
uint64 sys_exit(void);
uint64 sys_fork(void);
uint64 sys_wait(void);
uint64 sys_sbrk(void);
// uint64 sys_write(void);
void argint(int n, int *ip);
void argaddr(int n, uint64 *ip);
int argstr(int n, char *buf, int max);
int fork(void);
void exit(int status);
int wait(uint64 addr);

/* ======================================================
 * Lab6 新增：自旋鎖
 * 文件：kernel/sync/spinlock.c
 * ====================================================== */
 void initlock(struct spinlock *lk, char *name);
 void acquire(struct spinlock *lk);
 void release(struct spinlock *lk);
 int  holding(struct spinlock *lk);
 void push_off(void);
 void pop_off(void);
 
 /* ======================================================
  * Lab6 新增：睡眠鎖
  * 文件：kernel/sync/sleeplock.c
  * ====================================================== */
 void initsleeplock(struct sleeplock *lk, char *name);
 void acquiresleep(struct sleeplock *lk);
 void releasesleep(struct sleeplock *lk);
 int  holdingsleep(struct sleeplock *lk);

/* ======================================================
 * Lab7 新增：块缓冲层
 * 文件：kernel/fs/bio.c
 * ====================================================== */
void binit(void);
void virtio_disk_init(void);
struct buf *bread(uint dev, uint blockno);
void bwrite(struct buf *b);
void brelse(struct buf *b);
void fsinit(int dev);
void loginit(int dev);
void begin_op(void);
void end_op(void);
void log_write(struct buf *b);
void recover_from_log(void);

/* ======================================================
 * Lab7 新增：文件系统核心
 * 文件：kernel/fs/fs.c
 * ====================================================== */
void fsinit(int dev);
struct inode *iget(uint dev, uint inum);
struct inode *dirlookup(struct inode *dp, char *name, uint *poff);
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iput(struct inode *ip);
void iupdate(struct inode *ip);
struct inode *ialloc(uint dev, short type);
int dirlink(struct inode *dp, char *name, uint inum);
struct inode *namei(char *path);
struct inode *nameiparent(char *path, char *name);

/* ======================================================
 * Lab7 新增：文件描述符层
 * 文件：kernel/fs/file.c
 * ====================================================== */
struct file *filealloc(void);
struct file *filedup(struct file *f);
void fileclose(struct file *f);
int fileread(struct file *f, uint64 addr, int n);
int filewrite(struct file *f, uint64 addr, int n);

/* ======================================================
 * Lab7 新增：文件系统调用
 * 文件：kernel/syscall/sysfile.c
 * ====================================================== */
uint64 sys_open(void);
uint64 sys_read(void);
uint64 sys_write(void);
uint64 sys_close(void);
uint64 sys_unlink(void);

#endif /* DEFS_H */
