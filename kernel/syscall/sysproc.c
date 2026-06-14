/* sysproc.c — 系统调用内核实现（Lab6 任务4）
 *
 * 每个 sys_xxx() 函数是对应系统调用的真正内核实现。
 * 它们不接受参数（参数通过陷阱帧的寄存器传入，用 argint/argaddr 读取），
 * 返回 uint64 类型的结果值。
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "proc.h"
#include "riscv.h"
#include "types.h"

/* ================================================================
 * sys_getpid — 返回当前进程的 PID
 *
 * 对应的用户接口：int getpid(void)
 *
 * 实现很简单：调用 myproc() 获取当前进程的 PCB，
 * 然后返回它的 pid 字段。
 * ================================================================ */
uint64 sys_getpid(void) {
  /* ================================================================
   * TODO [Lab6-任务4-步骤1]：
   *   调用 myproc() 获取当前进程的 PCB 指针，返回其 pid 字段。
   * ================================================================ */
   struct proc *p = myproc();
   return p->pid;
}

/* ================================================================
 * sys_exit (Lab6 扩展)
 *   实现进程退出。简化版：打印退出信息，将进程状态设为 TASK_ZOMBIE，然后切回调度器。
 * ================================================================ */
 uint64 sys_exit(void) {
  int n;
  argint(0, &n);
  exit(n);
  return 0; // 不会执行到
}


uint64 sys_fork(void) {
  return fork();
}

uint64 sys_wait(void) {
  uint64 p;
  argaddr(0, &p);    // 用户传进来的 &status 地址
  return wait(p);
}

uint64 sys_exec(void) {
  char path[MAXPATH];
  uint64 argv;

  if (argstr(0, path, sizeof(path)) < 0)
    return -1;
  argaddr(1, &argv);

  return exec(path, argv);
}