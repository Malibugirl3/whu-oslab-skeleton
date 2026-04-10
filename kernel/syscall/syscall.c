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
#include "types.h"

/* 系统调用号常量定义 */
#define SYS_fork 1
#define SYS_exit 2
#define SYS_wait 3
#define SYS_getpid 11
#define SYS_sbrk 12
#define SYS_write 16

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
static uint64 (*syscalls[20])(void) = {
    /* [SYS_getpid] = sys_getpid, */ /* <-- 取消注释并添加这行 */
};

/* ================================================================
 * syscall — 系统调用分发主函数（由 usertrap 调用）
 * ================================================================ */
void syscall(void) {
  struct proc *p = myproc();

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
}
