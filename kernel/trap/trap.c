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
#include "proc.h"

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
  *(uint32*)PLIC_SCLAIM(hart) = 0;                // 清除 CLAIM 寄存器
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
  uint64 sepc = r_sepc(); // 获取当前程序计数器
  uint64 sstatus = r_sstatus(); // 获取当前状态寄存器
  uint64 scause = r_scause(); // 获取当前中断原因

  /* 验证：进入内核陷阱前，S-Mode 的中断应该已经关闭 */
  // if ((sstatus & SSTATUS_SPP) == 0)
  //   panic("sys_trap_handler: not from supervisor mode");
  if (intr_get()) // 如果中断使能，则panic
    panic("sys_trap_handler: entered with interrupts enabled");

  if (scause & 0x8000000000000000L) {
    /* 这是一个异步中断 */
    uint64 irq = scause & 0xff;

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
      static int ticks = 0; // 时钟中断次数
      ticks++; // 时钟中断次数加1
      if (ticks % 10 == 0) { // 每10次时钟中断打印一次
          printf("Tick! (%d)\n", ticks);
      }
      if (myproc() && myproc()->status == TASK_RUNNING) { // 如果当前有正在运行的进程，则调用yield
        yield();
      }
      break; // 退出中断处理

    case 9:{
      /* 外部中断（如 UART 键盘）：Lab7 之前可暂不处理 */
      int hart = r_tp();
      int plic_irq = *(uint32*)PLIC_SCLAIM(hart);
      if (plic_irq == UART0_IRQ) {
        char c = *(volatile char*)UART0;     // 读 UART 收到的字符
        uart_putc(c);  
      }
      *(uint32*)PLIC_SCLAIM(hart) = plic_irq;
      break;
    }
    default:
      printf("sys_trap_handler: unknown interrupt irq=%d\n", irq);
      break;
    }

  } else {
    uint64 cause = scause & 0xff;
    if (cause == 8) {
      intr_on();
      usertrap();
    } else {
      printf("sys_trap_handler: exception! scause=%d, sepc=%p, stval=%p\n",
             scause, sepc, r_stval());
      panic("sys_trap_handler: unexpected exception");
    }
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

  uint64 scause = r_scause();

  if (scause & 0x8000000000000000L) {
    uint64 irq = scause & 0xff;
    switch (irq) {
      case 1:
        /* ---- 时钟中断（从用户态触发的软件中断）----
         * timervec 向 sip.SSIP 写1 → CPU 在用户态感知到 → 跳到 usertrap */
        w_sip(r_sip() & ~SIP_SSIP);   /* 清除 SSIP，防止无限重触发 */
        if (myproc())
          myproc()->trapframe->epc = r_sepc();
        static int ticks = 0;
        ticks++;
        if (ticks % 10 == 0) 
          printf("U-Mode Tick! (%d)\n", ticks);
        if (myproc() && myproc()->status == TASK_RUNNING)
          yield();
        break;
      
      case 9:{
        int hart = r_tp();
        int plic_irq = *(uint32*)PLIC_SCLAIM(hart);
        if (plic_irq == UART0_IRQ) {
          char c = *(volatile char*)UART0;     // 读 UART 收到的字符
          uart_putc(c);  
        }
        *(uint32*)PLIC_SCLAIM(hart) = plic_irq;
        break;
      }
      default:
        printf("usertrap: unknown interrupt irq=%d\n", irq);
        panic("usertrap: unknown interrupt");
    }
      // 
    usertrapret();
    return;
  
  } else {
    uint64 irq2  = scause & 0xff;
    switch (irq2) {
      case 8:
        myproc()->trapframe->epc = r_sepc() + 4;

        intr_on();

        /* ================================================================
        * TODO [Lab6-任务2]：
        *   将被打断的 PC（sepc）向后移动 4 字节，跳过 ecall 指令。
        *   需要通过 myproc()->trapframe->epc 访问该字段并对其加 4。
        *   如不执行此步，返回后用户态会无限重复执行 ecall！
        * ================================================================ */

        /* 分发给系统调用处理函数 */
        syscall();

        usertrapret();
        return;
        /*
        * TODO [Lab6-任务]：将下方的临时逻辑替换为 syscall() 通用分发器。
        * 当前直接读取 a7 并 hardcode 处理，仅用于 proczero 初始化验证。
        * Lab6 中应改为：syscall();
        */


      //   uint64 num;
      //   asm volatile("mv %0, a7" : "=r"(num));
      //   // printf("================ usertrap: ecall ===============\n");

      //   if (num == 1) 
      //     printf("[proczero] first ecall! pid=%d\n", myproc()->pid);
      //   else if (num == 2) 
      //     printf("[proczero] second ecall! pid=%d\n", myproc()->pid);

      //   usertrapret();
      //   break;
      // default:
      //   printf("usertrap: unknown interrupt irq=%d\n", irq2);
      //   panic("usertrap: else p unknown interrupt");
      default:
        printf("usertrap sync exception: scause=%p sepc=%p stval=%p pid=%d\n",
               r_scause(), r_sepc(), r_stval(),
               myproc() ? myproc()->pid : -1);
        break;
    }
    panic("usertrap: unexpected exception");
  }
}
