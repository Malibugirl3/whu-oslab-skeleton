/* proc.h — 进程相关数据结构定义（Lab5 新增头文件）
 *
 * 在这里定义了操作系统最核心的两个数据结构：
 *   - struct context  : 进程在调度切换时保存的内核寄存器
 *   - struct trapframe: 进程被中断/系统调用时保存的所有寄存器
 *   - struct proc     : 进程控制块（Process Control Block，PCB）
 */
#ifndef PROC_H
#define PROC_H

#include "types.h"

/* ================================================================
 * 进程状态枚举
 * ================================================================ */
enum task_status {
  TASK_FREE,      /* 未使用的进程表槽位 */
  TASK_ALLOCATED, /* 已分配但尚未就绪（初始化中）*/
  TASK_READY,     /* 就绪态：等待调度器选中并运行 */
  TASK_RUNNING,   /* 运行态：当前正在某个 CPU 上执行 */
  TASK_SLEEPING,  /* 阻塞态：在等待某个事件（如 I/O 完成）*/
  TASK_ZOMBIE,    /* 僵尸态：已退出但父进程尚未调用 wait() 回收 */
};

/* ================================================================
 * struct context — 内核上下文（进程自愿让出 CPU 时保存的寄存器）
 *
 * 注意：这里只保存 Callee-Saved 寄存器（s0-s11, ra, sp）！
 * 原因：swtch() 本身是一个 C 函数调用，按照 RISC-V ABI，
 *       Caller-Saved 寄存器（t0-t6, a0-a7）由调用者自己保存，
 *       在发起 swtch() 调用前已经由编译器自动保存了。
 * ================================================================ */
struct context {
  /* Callee-saved 通用寄存器 */
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
  uint64 ra; /* 返回地址（swtch 返回后 ret 会跳到这里）*/
  uint64 sp; /* 内核栈指针 */
};

/* ================================================================
 * struct trapframe — 陷阱帧（进程被中断/ecall 时保存的所有寄存器）
 *
 * 每个进程独享一个 trapframe 页面（由 uvmcreate 分配）。
 * 当进程从用户态陷入内核时，所有寄存器保存在这里。
 * 当内核通过 sret 返回用户态时，从这里恢复所有寄存器。
 *
 * 字段顺序与 trampoline.S 中的 ld/sd 指令保持严格一致。
 * ================================================================ */
struct trapframe {
  uint64 kernel_satp;   /* 内核页表（用于陷入时切换地址空间）*/
  uint64 kernel_sp;     /* 该进程的内核栈顶地址 */
  uint64 kernel_trap;   /* usertrap() 函数的地址 */
  uint64 epc;           /* 用户态程序计数器（ecall 指令的地址）*/
  uint64 kernel_hartid; /* 当前 CPU 核心 ID */
  /* 以下是用户态的通用寄存器备份（按 x1-x31 顺序）*/
  uint64 ra;
  uint64 sp;
  uint64 gp;
  uint64 tp;
  uint64 t0;
  uint64 t1;
  uint64 t2;
  uint64 s0;
  uint64 s1;
  uint64 a0;
  uint64 a1;
  uint64 a2;
  uint64 a3;
  uint64 a4;
  uint64 a5;
  uint64 a6;
  uint64 a7;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
  uint64 t3;
  uint64 t4;
  uint64 t5;
  uint64 t6;
};

/* ================================================================
 * struct proc — 进程控制块（Process Control Block，PCB）
 *
 * 每个进程占用进程表（proc[]数组）中的一个槽位。
 * 进程的所有元信息都记录在这里。
 * ================================================================ */
struct proc {
  enum task_status status;     /* 进程当前状态 */
  int pid;                     /* 进程 ID（从 1 开始递增分配）*/
  pagetable_t pagetable;       /* 该进程的用户页表 */
  struct trapframe *trapframe; /* 陷阱帧（保存用户寄存器）*/
  struct context context;      /* 内核上下文（swtch 使用）*/
  uint64 kstack;               /* 该进程的内核栈顶地址 */
  uint64 sz;                   /* 进程地址空间大小（字节）*/
  char name[16];               /* 进程名称（调试用）*/
};

/* CPU 描述结构 */
struct cpu {
  struct proc *proc;      /* 当前在该 CPU 上运行的进程（如果有）*/
  struct context context; /* 调度器（scheduler）的内核上下文 */
  int noff;               /* 关中断的嵌套层数（push/pop intr）*/
  int intena;             /* 关中断前，中断是否是开着的 */
};

extern struct cpu
    cpus[NCPU]; /* 所有 CPU 核心的描述符数组（param.h 中 NCPU=1）*/
extern struct proc proc[NPROC]; /* 全局进程表（param.h 中 NPROC=64）*/

#endif /* PROC_H */
