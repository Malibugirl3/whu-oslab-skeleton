/* start.c — M-Mode 启动初始化（Lab4 新增文件）
 *
 * RISC-V 启动时处于最高特权级 M-Mode（Machine Mode）。
 * 本文件的任务：在 M-Mode 完成必要初始化后，降权到 S-Mode，
 * 然后跳入 start_main() 开始真正的内核工作。
 *
 * 为什么需要从 M-Mode 跳到 S-Mode？
 *   因为 RISC-V 的时钟中断（CLINT）只能在 M-Mode 配置，
 *   但后续的内核中断处理更适合在 S-Mode 进行。
 *   所以我们在 M-Mode 设置好时钟后，将后续工作委托给 S-Mode。
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

/* 时钟中断处理代码的入口（汇编实现），在 M-Mode 触发时调用 */
extern char timervec[];

/* 每个 CPU 核心的临时存储区，timervec 汇编代码需要用到 */
uint64 timer_scratch[NCPU][5];

/* ================================================================
 * timerinit — 配置 CLINT 硬件定时器，使其定期产生 M-Mode 时钟中断
 *
 * 原理：
 *   CLINT 中的 mtime 寄存器一直在递增（硬件驱动）。
 *   当 mtime 超过 mtimecmp 时，触发 M-Mode 时钟中断。
 *   我们的 timervec 汇编入口：截获这个中断 → 把 mtimecmp 推迟 interval →
 *   向 S-Mode 注入一个软件中断（相当于通知 S-Mode 时钟到了）。
 * ================================================================ */
void timerinit(void) {
  /* 获取当前 CPU 核心编号：在 entry.S 中被存入了 tp 寄存器 */
  int hartid = r_mhartid();

  /* 设置每次时钟中断的间隔：约 0.1 秒（具体时间取决于 QEMU 的时钟频率）*/
  int interval = 1000000;

  /* ================================================================
   * TODO [Lab4-任务4-步骤1]：安排第一次时钟中断触发时刻
   *
   * 目标：向 CLINT 硬件的 mtimecmp 寄存器写入"当前时间 + 间隔"，
   *   使定时器在 interval 时钟周期后首次产生 M-Mode 时钟中断。
   *
   * 完成前请先思考以下问题：
   *   - CLINT_MTIME 和 CLINT_MTIMECMP(hartid) 是什么类型的地址？
   *     （见 kernel/include/memlayout.h）
   *   - 为什么要用 (uint64*) 指针解引用来读写，而不是普通变量赋值？
   *   - 为什么每个 CPU 核心（hartid）需要独立的 mtimecmp？
   * ================================================================ */
  uint64 now = *(uint64*)CLINT_MTIME;
  
  *(uint64*)CLINT_MTIMECMP(hartid) = now + interval;

  /* 初始化 timer_scratch 暂存区（此段代码已提供，无需修改）
   * timervec 汇编在 M-Mode 处理时，通过 mscratch 找到此数组：
   *   scratch[0 ~ 2] 是干什么的？ TODO
   *   scratch[3] = CLINT_MTIMECMP(hartid) 的地址
   *   scratch[4] = 时钟间隔值
   * timervec 用这两个值来安排下一次中断时刻（mtimecmp += interval）*/
  uint64 *scratch = &timer_scratch[hartid][0];
  scratch[3] = CLINT_MTIMECMP(hartid);
  scratch[4] = interval;
  w_mscratch((uint64)scratch);

  /* ================================================================
   * TODO [Lab4-任务4-步骤2]：注册 M-Mode 时钟中断的汇编处理入口
   *
   * 目标：将 timervec（M-Mode 专用汇编处理程序）的地址写入
   *   M-Mode 陷阱向量寄存器，使时钟中断到来时直接跳转到 timervec。
   *
   * 完成前请先思考：
   *   - M-Mode 的陷阱向量用哪个 CSR 寄存器？是 mtvec 还是 stvec？
   *     （查看 kernel/include/riscv.h 中的 w_mtvec）
   *   - timervec 已在文件顶部声明，如何将标签地址转为 uint64？
   * ================================================================ */
  w_mtvec((uint64)timervec);

  /* ================================================================
   * TODO [Lab4-任务4-步骤3]：使能 M-Mode 时钟中断
   *
   * 目标：打开 mie 寄存器的 MTIE 位，允许 M-Mode 接收 CLINT 时钟中断。
   *
   * 完成前请先思考：
   *   - 若不设置 MTIE，即使 mtimecmp 到达，CPU 会有何反应？
   *   - 为何必须使用"读-改-写"模式（r_mie() | MIE_MTIE），不能直接赋值？
   *     （MIE_MTIE 常量见 kernel/include/riscv.h）
   * ================================================================ */
  w_mie(r_mie() | MIE_MTIE);

  /* ================================================================
   * TODO [Lab4-任务4-步骤4]：开启 M-Mode 全局中断总开关
   *
   * 目标：打开 mstatus 寄存器的 MIE 位，这是 M-Mode 接收一切中断的总开关。
   *
   * 完成前请先思考：
   *   - mstatus.MIE 与 mie.MTIE 是什么关系？两者都需要设置吗？
   *     mstatus.MIE 是 M-Mode 全局中断使能位，mie.MTIE 是 M-Mode 时钟中断使能位，两者都需要设置。
   *   - MSTATUS_MIE 常量在 riscv.h 中对应哪个 bit？
   * 
   * ================================================================ */
  w_mstatus(r_mstatus() | MSTATUS_MIE);
}

/* ================================================================
 * start — M-Mode 主函数（在 entry.S 的栈设置完成后，立即被调用）
 *
 * 任务：
 *   1. 配置 mstatus：将"上一特权级"设为 S-Mode（MPP=01）
 *   2. 配置中断委托：把大多数中断/异常委托给 S-Mode 处理
 *   3. 初始化定时器
 *   4. 用 mret 指令降权跳入 S-Mode 的 start_main()
 * ================================================================ */
void start(void) {
  /* 将 "上一特权级" 设为 S-Mode，这样执行 mret 时会进入 S-Mode */
  uint64 x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK; /* 清除 MPP 字段 */
  x |= MSTATUS_MPP_S;     /* 设置 MPP = 01（S-Mode）*/
  w_mstatus(x);

  /* 设置 mepc 为 start_main 的地址，mret 执行后 PC 会跳到这里 */
  w_mepc((uint64)start_main);

  /* 将所有中断和异常委托给 S-Mode 处理（不需要 M-Mode 转手）*/
  w_medeleg(0xffff); /* 委托所有同步异常 */
  w_mideleg(0xffff); /* 委托所有中断 */

  /* 在 S-Mode 中开启时钟中断和软件中断的使能位 */
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  /* 初始化 M-Mode 定时器 */
  timerinit();

  /* 将当前 CPU 核心编号（hartid）保存在 tp 寄存器中 */
  /* （mycpu() 函数会通过读取 tp 来知道当前是哪个核心）*/
  w_tp(r_mhartid());

  /* 配置 PMP：赋予 S-Mode 对全部物理内存的读/写/执行权限 */
  /* pmpaddr0 的 TOR 模式覆盖地址 [0, 0x3ffffffffffff*4) ≈ 全部物理内存 */
  asm volatile("csrw pmpaddr0, %0" : : "r"(0x3fffffffffffffull));
  /* pmpcfg0 = 0x0F = R|W|X|TOR，全通行证 */
  asm volatile("csrw pmpcfg0, %0" : : "r"(0xf));

  /* 执行 mret：降权到 S-Mode，跳转到 mepc 所指的 start_main() */
  asm volatile("mret");

  /* 不会到达这里 */
}
