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


uint64 sys_write(void) {
  /* ================================================================
  * sys_write — Lab6 简化版标准输出
  *
  * 当前只支持 fd == 1，即标准输出/串口输出。
  * 参数通过 argint/argaddr 从 trapframe 中提取：
  *   arg0: fd
  *   arg1: 用户缓冲区地址 buf
  *   arg2: 写入长度 count
  *
  * 为避免用户传入超大 count 导致内核栈溢出，本实现使用小缓冲区
  * 分块 copyin，再逐字节输出到 UART。
  *
  * Lab7 引入文件系统后，这里应改为：
  *   1. 根据 fd 查找当前进程打开的 struct file；
  *   2. 调用 filewrite(f, buf, count)；
  *   3. 由 console/file 层分别处理终端输出和磁盘文件写入。
  * ================================================================ */


  int fd;
  uint64 buf;
  int count;

  argint(0, &fd); // 获取文件描述符
  argaddr(1, &buf); // 获取用户态地址
  argint(2, &count); // 获取写入字节数

  if (fd != 1 || count < 0)
    return -1;
  
  char kbuf[64];
  int written = 0;
    
  while (written < count) {
    int n = count - written;
    if (n > sizeof(kbuf))
      n = sizeof(kbuf);
    
    if (copyin(kernel_pagetable, kbuf, buf + written, n) < 0) {
      if (written == 0)
        return -1;
      break;
    }
    
    for (int i = 0; i < n; i++)
      uart_putc(kbuf[i]);
    
    written += n;
  }
    
  return written;

}

uint64 sys_fork(void) {
  return fork();
}

uint64 sys_wait(void) {
  uint64 p;
  argaddr(0, &p);    // 用户传进来的 &status 地址
  return wait(p);
}