/* proc.c — 进程管理（Lab5 任务1、3、4）
 *
 * 本文件实现了进程生命周期管理的核心逻辑：
 *   - procinit()   : 初始化进程表
 *   - allocproc()  : 为新进程分配 PCB
 *   - scheduler()  : 调度器主循环（无限轮询、找到就绪进程就运行）
 *   - yield()      : 当前进程主动放弃 CPU（配合时钟中断使用）
 */

#include "proc.h"
#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include "initcode.h"

extern char user_trap_vector[];
extern void userret(struct trapframe*);

/* 全局进程表和 CPU 描述符（在 proc.h 中 extern 声明）*/
struct proc proc[NPROC];
struct cpu cpus[NCPU];

/* 进程 ID 计数器（每次 allocpid 返回后递增）*/
static int nextpid = 1;
static struct spinlock pid_lock;
static struct spinlock wait_lock;
static struct proc *initproc;

static void freeproc(struct proc *p) {
  if (p->trapframe) {
    kfree((void*)p->trapframe);
    p->trapframe = 0;
  }
  if (p->kstack) {
    kfree((void*)p->kstack);
    p->kstack = 0;
  }

  if (p->pagetable) {
    uvmfree(p->pagetable, p->sz);
    p->pagetable = 0;
  }

  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->status = TASK_FREE;
}
/* ================================================================
 * mycpu — 获取当前 CPU 核心的 cpu 结构指针
 *
 * 实现方式：读取 tp 寄存器（在 start.c 中被设置为 hartid）
 * ================================================================ */
struct cpu *mycpu(void) {
  int hartid = r_tp(); 
  return &cpus[hartid]; // hartid是cpu的编号，从0开始，这个地方取出来的是对应CPU的cpu结构指针
}

void forkret(void) {
  usertrapret();
}

/* ================================================================
 * myproc — 获取当前 CPU 上正在运行的进程的 PCB 指针
 * 初始的时候假装之前运行过proczero进程 
 * ================================================================ */
struct proc *myproc(void) { return mycpu()->proc; }

/* ================================================================
 * allocpid — 分配一个唯一的进程 ID
 * ================================================================ */
int allocpid(void) {
  int pid;
  acquire(&pid_lock);
  pid = nextpid;
  nextpid++;
  release(&pid_lock);
  return pid;
}

/* ================================================================
 * procinit — 初始化进程表（内核启动时调用一次）
 *
 * 任务：将进程表中所有条目的状态初始化为 TASK_FREE。
 * ================================================================ */
void procinit(void) {
  initlock(&pid_lock, "pid_lock");
  initlock(&wait_lock, "wait_lock");
  for (int i = 0; i < NPROC; i++) {
    initlock(&proc[i].lock, "proc");
    proc[i].status = TASK_FREE;
    proc[i].chan = 0;
    proc[i].killed = 0;
    proc[i].xstate = 0;
    proc[i].parent = 0;
  }
}

/* ================================================================
 * allocproc — 在进程表中找一个空槽并初始化
 *
 * 返回：指向已初始化的 PCB 的指针；若进程表满，返回 0。
 *
 * 初始化内容：
 *   - 分配 pid
 *   - 将状态从 TASK_FREE 改为 TASK_ALLOCATED
 *   - 分配 trapframe 页（用于保存用户寄存器）
 *   - 初始化内核 context（ra 设为某个"进程首次被调度时跳入的地址"）
 * ================================================================ */
struct proc *allocproc(void) {
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->status == TASK_FREE) {
      p->status = TASK_ALLOCATED;
      p->pid = allocpid();
      p->trapframe = (struct trapframe *)kalloc();
      if (p->trapframe == 0) {
        p->status = TASK_FREE;
        release(&p->lock);
        return 0;
      }
      p->kstack = (uint64)kalloc();
      if (p->kstack == 0) {
        kfree((void*)p->trapframe);
        p->trapframe = 0;
        p->status = TASK_FREE;
        release(&p->lock);
        return 0;
      }
      p->context.sp = p->kstack + PGSIZE;
      memset(p->trapframe, 0, sizeof(struct trapframe));
      p->context.ra = (uint64)forkret;
      p->pagetable = 0;
      p->chan = 0;
      p->killed = 0;
      p->xstate = 0;
      p->parent = 0;
      for (int i = 0; i < NOFILE; i++)
        p->ofile[i] = 0;
      release(&p->lock);
      return p;
    }
    release(&p->lock);
  }
  return 0;
}

/* ================================================================
 * scheduler — 调度器主循环（永不返回！）
 *
 * 这是操作系统的"上帝"：它在所有进程之间无限轮转，
 * 当看到一个 TASK_READY 的进程时，就把 CPU 交给它。
 *
 * 流程：
 *   for 每次循环:
 *     1. 打开全局中断（防止系统无法接收时钟信号而死锁）
 *     2. 遍历进程表，找到 TASK_READY 的进程
 *     3. 将该进程标记为 TASK_RUNNING
 *     4. 调用 swtch，从调度器上下文切换到进程的内核上下文
 *     5. 当进程放弃 CPU（yield/sleep/exit）后，swtch 返回到这里
 *     6. 清除 mycpu()->proc，继续找下一个
 * ================================================================ */
void scheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0; // 初始的时候假装之前运行过proczero进程

  for (;;) {
    /* 必须打开中断！否则时钟信号无法到达，调度无法触发 */
    intr_on();

    for (p = proc; p < &proc[NPROC]; p++) {
      /* ================================================================
       * TODO [Lab5-任务3]：
       *   完成调度器核心逻辑：
       *   1. 检查 p->status == TASK_READY
       *   2. 将状态改为 TASK_RUNNING
       *   3. 将 c->proc 设为 p
       *   4. 调用 swtch 切换到 p 的上下文：swtch(&c->context, &p->context)
       *   5. swtch 返回后（进程放弃了CPU），清零 c->proc
       * ================================================================ */
      acquire(&p->lock);
      if (p->status == TASK_READY) {
        p->status = TASK_RUNNING;
        c->proc = p;
        release(&p->lock);
        swtch(&c->context, &p->context);
        c->proc = 0;
      } else {
        release(&p->lock);
      }
    }
  }
}

/* ================================================================
 * yield — 当前进程主动放弃 CPU（由时钟中断处理函数调用）
 *
 * 过程：将自己的状态从 TASK_RUNNING 改回 TASK_READY，然后切回调度器。
 * ================================================================ */
void yield(void) {
  struct proc *p = myproc();

  acquire(&p->lock);
  p->status = TASK_READY;
  release(&p->lock);

  swtch(&p->context, &mycpu()->context);
}

/* ================================================================
 * sleep — 当前进程进入睡眠状态，等待某个事件发生
 * 
 * 过程：将自己的状态从 TASK_RUNNING 改回 TASK_SLEEPING，然后切回调度器。
 * ================================================================ */
void sleep(void *chan, struct spinlock *lk) {
  struct proc *p = myproc();

  acquire(&p->lock);
  release(lk);

  p->chan = chan;
  p->status = TASK_SLEEPING;

  release(&p->lock);

  swtch(&p->context, &mycpu()->context);

  acquire(&p->lock);
  p->chan = 0;
  release(&p->lock);

  acquire(lk);
}



void wakeup(void *chan) {
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++) {
    if (p == myproc())
      continue;
    acquire(&p->lock);
    if (p->status == TASK_SLEEPING && p->chan == chan) {
      p->status = TASK_READY; 
    }
    release(&p->lock);
  }
}

void exit(int status) {
  struct proc *p = myproc();
  struct proc *pp;  // 遍历进程表

  if (p == initproc)
    panic("initproc exiting");

  for (int fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd]) {
      fileclose(p->ofile[fd]);
      p->ofile[fd] = 0;
    }
  }

  acquire(&wait_lock);

  // 把孤儿进程挂到 initproc，如果有子进程的话
  for (pp = proc; pp < &proc[NPROC]; pp++) {
    if (pp->parent == p) {
      pp->parent = initproc;
      wakeup(initproc);   // 如果 init 正在 wait，唤醒它
    }
  }
  // 唤醒正在 wait 自己的父进程
  wakeup(p->parent);

  acquire(&p->lock);
  p->xstate = status;
  p->status = TASK_ZOMBIE;
  release(&p->lock);
  release(&wait_lock);

  // 让出 CPU，不应返回
  swtch(&p->context, &mycpu()->context);
  panic("zombie exit");
}

int wait(uint64 addr) {
  struct proc *p = myproc();
  struct proc *pp;
  int havekids, pid;

  acquire(&wait_lock);
  for (;;) {
    havekids = 0;

    for (pp = proc; pp < &proc[NPROC]; pp++) {
      if (pp->parent != p)
        continue;

      havekids = 1;
      acquire(&pp->lock);

      if (pp->status == TASK_ZOMBIE) {
        pid = pp->pid;

        // 可选：后面你实现 copyout 后再把 xstate 回写给用户
        // if (addr != 0 && copyout(..., &pp->xstate, sizeof(pp->xstate)) < 0) { ... }
        if (addr !=0) {
          if (copyout(p->pagetable, addr, (char*)&pp->xstate, sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
        }

        freeproc(pp);
        release(&pp->lock);
        release(&wait_lock);
        return pid;
      }

      release(&pp->lock);
    }

    if (!havekids || p->killed) {
      release(&wait_lock);
      return -1;
    }

    sleep(p, &wait_lock);   // 会原子释放 wait_lock 并睡眠，醒来后重新持有
  }
}


int fork(void) {
  int pid;
  struct proc *p = myproc();
  struct proc *np = allocproc();
  if (np == 0) {
    return -1;
  }

  np->pagetable = uvmcreate();
  if (np->pagetable == 0) {
    freeproc(np);
    return -1;
  }

  if (uvmcopy(p->pagetable, np->pagetable, p->sz) != 0) {
    freeproc(np);
    return -1;
  }

  np->sz = p->sz;
  memmove(np->trapframe, p->trapframe, sizeof(*p->trapframe));
  np->trapframe->a0 = 0;

  np->parent = p;
  memmove(np->name, p->name, sizeof(np->name));
  for (int i = 0; i < NOFILE; i++) {
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  }

  pid = np->pid;

  acquire(&np->lock);
  np->status = TASK_READY;
  release(&np->lock);

  return pid;
}




void userinit(void) {
  struct proc *p = allocproc();
  if (p == 0) 
    panic("userinit: allocproc failed");

  if (user_initcode_bin_len > PGSIZE) 
    panic("userinit: initcode too large for one page");

  p->pagetable = uvmcreate();
  if (p->pagetable == 0)
    panic("userinit: uvmcreate failed");

  char *mem = kalloc(); // 分配一页内存，用于存放proczero_code，返回的是虚拟地址
  if (mem == 0)
    panic("userinit: mem alloc failed");

  memset(mem, 0, PGSIZE);  
  memmove(mem, user_initcode_bin, user_initcode_bin_len);

  if (mappages(p->pagetable, (uint64)mem, 0, PGSIZE,
                PTE_R | PTE_W | PTE_X | PTE_U) != 0)
    panic("userinit: map initcode failed");
  
  sfence_vma(); // 刷新TLB

  // 默认从0开始执行
  p->trapframe->epc = 0;
  p->trapframe->sp = PGSIZE;
  p->sz = PGSIZE;

  memset(p->name, 0, sizeof(p->name));
  memmove(p->name, "proczero", 9);

  p->status = TASK_READY;
  initproc = p;
}

void usertrapret(void) {
  struct proc *p = myproc();

  intr_off();

  w_stvec((uint64)user_trap_vector);
  w_sscratch((uint64)p->trapframe); // 保存trapframe地址到sscratch

  p->trapframe->kernel_satp = MAKE_SATP(kernel_pagetable);
  p->trapframe->kernel_sp = p->kstack + PGSIZE;
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();

  uint64 x = r_sstatus();
  x &= ~SSTATUS_SPP;  // 清除 SPP 位，表示返回用户态
  x |= SSTATUS_SPIE;  // 设置 SPIE 位，表示返回用户态时中断使能
  w_sstatus(x);

  w_sepc(p->trapframe->epc);

  w_sip(r_sip() & ~SIP_SSIP); // 清除 SSIP 位，防止无限重触发


  w_satp(MAKE_SATP(p->pagetable));
  sfence_vma();
  userret(p->trapframe);
  // // asm volatile("sret");
  // asm volatile(
  //     "mv sp, %0\n"
  //     "sret\n"
  //     : : "r"(p->kstack + PGSIZE)
  // );
}