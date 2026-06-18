/* syscall.c — 系统调用分发（Lab6 任务3）
 *
 * 当用户程序执行 ecall 并由 usertrap() 捕获后，
 * 调用本文件的 syscall() 函数进行分发：
 *   1. 从陷阱帧读取系统调用号（a7 寄存器的值）
 *   2. 在函数指针表中查找对应的内核实现函数
 *   3. 调用该函数，将返回值写回陷阱帧的 a0 寄存器
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "userabi.h"
#include "types.h"

/* 获取定义长度的宏 */
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

/* ================================================================
 * TODO [Lab6-任务3-步骤1]：
 *   完善系统调用函数指针表 syscalls[]。
 *
 *   工作原理：
 *     syscalls[11] = sys_getpid
 *     当用户程序将 a7=11 并执行 ecall 时，
 *     syscall() 会调用 syscalls[11]()，即 sys_getpid()。
 *
 *   目前只实现 sys_getpid，其余留空（NULL）。
 *   后续可按需添加更多系统调用。
 * ================================================================ */
extern uint64 sys_open(void);
extern uint64 sys_read(void);
extern uint64 sys_unlink(void);
extern uint64 sys_close(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_chdir(void);
extern uint64 sys_fstat(void);

static uint64 (*syscalls[NELEM_SYSCALL])(void) = {
    [SYS_fork]   = sys_fork,
    [SYS_exit]   = sys_exit,
    [SYS_wait]   = sys_wait,
    [SYS_open]   = sys_open,
    [SYS_read]   = sys_read,
    [SYS_write]  = sys_write,
    [SYS_close]  = sys_close,
    [SYS_unlink] = sys_unlink,
    [SYS_exec]   = sys_exec,
    [SYS_getpid] = sys_getpid,
    [SYS_mkdir]  = sys_mkdir,
    [SYS_chdir]  = sys_chdir,
    [SYS_fstat]  = sys_fstat,
};

/* ================================================================
 * syscall — 系统调用分发主函数（由 usertrap 调用）
 * ================================================================ */
void syscall(void) {
  struct proc *p = myproc();
  p->trapframe->epc += 4;

  /* 从陷阱帧读取系统调用号（用户在 a7 寄存器中填入的值）*/
  int num = p->trapframe->a7;

  /* ================================================================
   * TODO [Lab6-任务3-步骤2]：
   *   1. 检查 num 是否在合法范围内（1 <= num < NELEM(syscalls)），
   *      且 syscalls[num] 不为 NULL。
   *   2. 若合法，调用 syscalls[num]()，
   *      将返回值存入 p->trapframe->a0（用户程序会从 a0 读取返回值）。
   *   3. 若非法，打印错误并将 p->trapframe->a0 = -1（返回错误码）。
   * ================================================================ */
   if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
      p->trapframe->a0 = syscalls[num]();
   } else {
      printf("unknown syscall %d\n", num);
      p->trapframe->a0 = -1;
   }
}

/*
 * 获取参数
 * 参数：
 *   n：参数索引
 * 返回：
 *   参数值
 */
static uint64 argraw(int n) {
  struct proc *p = myproc();

  switch (n) {
    case 0:
      return p->trapframe->a0;
    case 1:
      return p->trapframe->a1;
    case 2:
      return p->trapframe->a2;
    case 3:
      return p->trapframe->a3;
    case 4:
      return p->trapframe->a4;
    case 5:
      return p->trapframe->a5;
    default:
      printf("argraw: invalid n=%d\n", n);
      panic("argraw");
  }
}

/*
 * 获取整数参数
 * 参数：
 *   n：参数索引
 *   ip：保存结果的整数指针
*/
void argint(int n, int *ip) {
  *ip = (int)argraw(n);
}

/*
 * 获取地址参数
 * 参数：
 *   n：参数索引
 *   ip：保存结果的地址指针
 */
void argaddr(int n, uint64 *ip) {
  *ip = argraw(n);
}

/*
 * 获取字符串参数
 * 参数：
 *   n：参数索引
 *   buf：用户缓冲区地址
 *   max：最大长度
 * 返回：
 *   0：成功
 *   -1：失败
 */
int argstr(int n, char *buf, int max) {
  struct proc *p = myproc();
  
  uint64 addr;
  argaddr(n, &addr);

  if (copyinstr(p->pagetable, buf, addr, max) < 0)  // 先打通
    return -1;
  
  return 0;
}