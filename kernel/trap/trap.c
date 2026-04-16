/* trap.c — 中断与异常分发（Lab4 任务1&3，Lab6 扩展）
 *
 * 本文件是内核的"中控室"。当 sys_trap_vector 把寄存器保存完毕，
 * 就会调用 sys_trap_handler()，由它来判断发生了什么事并分派处理。
 *
 * Lab4 实现：处理时冲中断，每次打印 "Tick!"
 * Lab5 扩展：在时钟中断中增加 yield()，触发进程调度
 * Lab6 扩展：增加 usertrap()，处理来自用户态的 ecall 系统调用
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

/* 声明 sys_trap_vector 汇编入口（在 kernelvec.S 中定义）*/
extern char sys_trap_vector[];

/* ================================================================
 * trapinithart — 设置 S-Mode 陷阱向量
 *
 * 告诉 CPU：当 S-Mode 下发生中断/异常时，跳转到 sys_trap_vector。
 * 在 main.c 的 start_main() 中调用一次即可（每个 CPU 核心调用一次）。
 * ================================================================ */
void trapinithart(void) {
  /* ================================================================
   * TODO [Lab4-任务1]：注册 S-Mode 陷阱向量入口
   *
   * 目标：告诉 CPU，当 S-Mode 下发生中断或异常时，应跳转到哪个地址开始处理。
   *
   * 你需要回答以下问题后，再着手实现：
   *   1. 哪个 CSR 寄存器存放 S-Mode 陷阱处理入口地址？
   *      （提示：查阅 kernel/include/riscv.h 中以 w_s 开头的写函数）
   *   2. sys_trap_vector 是汇编中定义的一个地址标签，已在本文件顶部声明为
   *      extern char sys_trap_vector[]。如何从 C 中取得它的地址并转换为 uint64？
   *   3. 陷阱向量寄存器有「直接模式」和「向量模式」两种，本框架使用哪种？
   * ================================================================ */
  w_stvec((uint64)sys_trap_vector);
}

void plicinit(void) {
  int hart = 0; // 当前 CPU 核心编号
  *(uint32*)(PLIC_PRIORITY + UART0_IRQ * 4) = 1;  // UART 优先级设为1
  *(uint32*)PLIC_SENABLE(hart) = (1 << UART0_IRQ); // 使能 IRQ10
  *(uint32*)PLIC_SPRIORITY(hart) = 0;              // 阈值设为0
}

/* ================================================================
 * sys_trap_handler — 内核态中断/异常总处理函数（由 sys_trap_vector 汇编调用）
 *
 * 本函数从 CSR 读取中断原因，然后根据类型分发处理：
 *
 *   scause 最高位（bit 63）：
 *     = 1 → 异步中断（Interrupt），低位表示具体类型
 *     = 0 → 同步异常（Exception），不应在内核中发生
 *
 *   常见中断类型（irq 值）：
 *     1  → 软件中断（由 M-Mode 的 timervec 注入的时钟信号）
 *     5  → S-Mode 时钟中断（如果直接委托到 S-Mode）
 *     9  → 外部中断（UART 键盘输入等）
 * ================================================================ */
void sys_trap_handler(void) {
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  // printf("sys_trap_handler: sepc=%p, sstatus=%p, scause=%p\n", sepc, sstatus, scause);

  /* 验证：进入内核陷阱前，S-Mode 的中断应该已经关闭 */
  if ((sstatus & SSTATUS_SPP) == 0)
    panic("sys_trap_handler: not from supervisor mode");
  if (intr_get())
    panic("sys_trap_handler: entered with interrupts enabled");

  if (scause & 0x8000000000000000L) {
    /* 这是一个异步中断 */
    uint64 irq = scause & 0xff;
    // printf("sys_trap_handler: scause=%d\n", scause);


    switch (irq) {
    case 1:
      /* ================================================================
       * TODO [Lab4-任务3-步骤1]：处理时钟软件中断（scause irq=1）
       *
       * 背景：M-Mode 的 timervec 汇编代码在处理硬件时钟中断后，
       *   通过向 sip 寄存器的 SSIP 位写 1，向 S-Mode 注入一个软件中断信号。
       *   本 case 分支就是响应该信号的地方。
       *
       * 你的任务（不给实现，只给目标）：
       *   1. 清除中断待处理标志：sip 寄存器的哪一位对应软件中断 pending？
       *      若不清除，会发生什么情况？
       *      参考：kernel/include/riscv.h 中的 r_sip() 和 w_sip()
       *   2. 统计时钟中断次数（ticks），并以适当频率打印心跳信息。
       *      思考：为什么不应每次中断都打印？应如何控制打印频率？
       *   3. （Lab5 完成后追加）：若当前有正在运行的进程，调用 yield() 让出 CPU。
       * ================================================================ */
      w_sip(r_sip() & ~SIP_SSIP); // 清除软件中断待处理标志

      static int ticks = 0;
      ticks++;
      if (ticks % 10 == 0) {
          printf("Tick! (%d)\n", ticks);
      }
      break;

    case 9:{
      /* 外部中断（如 UART 键盘）：Lab7 之前可暂不处理 */
      // printf("sys_trap_handler: scause=%d\n", scause);

      // printf("===================================dadvadv \n");
      // int hart = r_mhartid(); // 这里有问题会保持奇怪的错误 sys_trap_handler: exception! scause=2, sepc=80000df6, stval=f14027f3
      // 分析：原本的代码是直接赋值 hart=0 为什么可以运行？ 因为我们这个os只有一个内核所以直接赋值没问题，但是我觉得这样不是很好
      // 因为假如说我们以后做成多核尤其我们可以很清楚的看到我们在空间的划分上还有很多地方都是使用hart编号来定位的数据也就是说这其实是一种为我们将来制作多核而进行的提前准备
      // 但是这里之所以不能够直接使用t_mhartid()是因为我们当前是在S_MODE下
      int hart = r_tp();
      // printf("===================================\n");
      int irq = *(uint32*)PLIC_SCLAIM(hart);
      // int irq = 10;
      // printf("sys_trap_handler: irq=%d\n", irq);
      if (irq == UART0_IRQ) {
        char c = *(volatile char*)UART0;     // 读 UART 收到的字符
        uart_putc(c);  
      }
      *(uint32*)PLIC_SCLAIM(hart) = irq;    // 通知PLIC中断已经处理，可以继续处理其他中断
      // while(1);
      break;
    }
    default:
      printf("sys_trap_handler: unknown interrupt irq=%d\n", irq);
      break;
    }

  } else {
    /* 同步异常：内核代码出了错，无法恢复，直接 panic */
    printf("sys_trap_handler: exception! scause=%d, sepc=%p, stval=%p\n",
           scause, sepc, r_stval());
    panic("sys_trap_handler: unexpected exception");
  }

  /* ================================================================
   * 恢复 sepc 和 sstatus：
   * 某些情况下（如嵌套中断）它们可能被修改过，需要还原。
   * ================================================================ */
  w_sepc(sepc);
  w_sstatus(sstatus);
}

/* ================================================================
 * usertrap — 用户态陷阱处理（Lab6 新增）
 *
 * 当用户程序执行 ecall 时，CPU 切换到 S-Mode 并调用此函数。
 *
 * 区别于 sys_trap_handler：
 *   - 需要切换陷阱向量到 sys_trap_vector（防止用户态 PC 出现在栈跟踪里）
 *   - 需要将 epc 加 4，跳过 ecall 指令（否则返回后又会执行 ecall）
 *   - 只处理 scause == 8（来自 U-Mode 的 ecall）
 * ================================================================ */
void usertrap(void) {
  /* 立即切换到内核态陷阱向量（防止处理用户陷阱时再发生用户态中断）*/
  w_stvec((uint64)sys_trap_vector);

  uint64 cause = r_scause();

  if (cause == 8) {
    /* 来自 U-Mode 的 ecall（系统调用）*/

    /* 允许中断（系统调用可能涉及耗时 I/O 操作）*/
    intr_on();

    /* ================================================================
     * TODO [Lab6-任务2]：
     *   将被打断的 PC（sepc）向后移动 4 字节，跳过 ecall 指令。
     *   需要通过 myproc()->trapframe->epc 访问该字段并对其加 4。
     *   如不执行此步，返回后用户态会无限重复执行 ecall！
     * ================================================================ */

    /* 分发给系统调用处理函数 */
    // syscall();

  } else {
    /* 用户态发生异常（如非法内存访问），直接终止该进程 */
    printf("usertrap: unexpected scause=%ld\n", cause);
    /* 理想情况下应该 exit(-1) 杀死该进程，暂不实现 */
    panic("usertrap");
  }
}
