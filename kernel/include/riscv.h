/* riscv.h — RISC-V 架构专用寄存器读写内联函数（已提供，无需修改）
 *
 * 这些函数通过 C 内联汇编直接操作 RISC-V CSR（控制状态寄存器）。
 * 它们是内核与硬件之间的"语言翻译官"。
 */
#ifndef RISCV_H
#define RISCV_H

#include "types.h"

/* ======== 页表相关常量（Lab3 使用）======== */
#define PTE_V (1L << 0) /* 有效位 Valid */
#define PTE_R (1L << 1) /* 可读 Readable */
#define PTE_W (1L << 2) /* 可写 Writable */
#define PTE_X (1L << 3) /* 可执行 Executable */
#define PTE_U (1L << 4) /* 用户态可访问 User-accessible */

/* 从虚拟地址 va 中提取第 level 级的 9 位 VPN（Virtual Page Number）索引 */
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
#define PX(level, va) ((((uint64)(va)) >> PXSHIFT(level)) & 0x1FF)

/* 物理页号（PPN）与页表项（PTE）之间的转换 */
#define PA2PTE(pa) ((((uint64)(pa)) >> 12) << 10)
#define PTE2PA(pte) (((pte) >> 10) << 12)
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

/* Sv39 分页模式的 satp 寄存器格式：mode=8 + 物理页号 */
#define MAKE_SATP(pagetable) (((uint64)8 << 60) | (((uint64)(pagetable)) >> 12))

/* ======== CSR 寄存器读写宏（Lab3-6 使用）======== */

/* 机器态状态寄存器 mstatus */
#define MSTATUS_MPP_MASK (3L << 11) /* MPP字段掩码 */
#define MSTATUS_MPP_M (3L << 11)    /* MPP=11 表示上一特权级是Machine */
#define MSTATUS_MPP_S (1L << 11)    /* MPP=01 表示上一特权级是Supervisor */
#define MSTATUS_MPP_U (0L << 11)    /* MPP=00 表示上一特权级是User */
#define MSTATUS_MIE (1L << 3)       /* Machine全局中断使能位 */

static inline uint64 r_mstatus() {
  uint64 x;
  asm volatile("csrr %0, mstatus" : "=r"(x));
  return x;
}
static inline void w_mstatus(uint64 x) {
  asm volatile("csrw mstatus, %0" : : "r"(x));
}

/* 机器态异常程序计数器 mepc：记录从M-Mode返回后应跳往的地址 */
static inline void w_mepc(uint64 x) {
  asm volatile("csrw mepc, %0" : : "r"(x));
}

/* 监管态状态寄存器 sstatus */
#define SSTATUS_SPP (1L << 8)  /* SPP: 上一特权级（0=用户态，1=内核态）*/
#define SSTATUS_SPIE (1L << 5) /* 上一次S态中断使能状态（返回后的中断开关）*/
#define SSTATUS_UPIE (1L << 4)
#define SSTATUS_SIE (1L << 1) /* S态全局中断使能位（当前是否允许中断）*/
#define SSTATUS_UIE (1L << 0)

static inline uint64 r_sstatus() {
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r"(x));
  return x;
}
static inline void w_sstatus(uint64 x) {
  asm volatile("csrw sstatus, %0" : : "r"(x));
}

/* 监管态中断挂起寄存器 sip */
#define SIP_SSIP (1L << 1)

static inline uint64 r_sip() {
  uint64 x;
  asm volatile("csrr %0, sip" : "=r"(x));
  return x;
}
static inline void w_sip(uint64 x) { asm volatile("csrw sip, %0" : : "r"(x)); }

/* 监管态中断使能寄存器 sie */
#define SIE_SEIE (1L << 9) /* 外部中断使能 */
#define SIE_STIE (1L << 5) /* 时钟中断使能 */
#define SIE_SSIE (1L << 1) /* 软件中断使能 */

static inline uint64 r_sie() {
  uint64 x;
  asm volatile("csrr %0, sie" : "=r"(x));
  return x;
}
static inline void w_sie(uint64 x) { asm volatile("csrw sie, %0" : : "r"(x)); }

/* 机器态中断使能寄存器 mie */
#define MIE_MEIE (1L << 11) /* 机器态外部中断使能 */
#define MIE_MTIE (1L << 7)  /* 机器态时钟中断使能 */
#define MIE_MSIE (1L << 3)  /* 机器态软件中断使能 */

static inline uint64 r_mie() {
  uint64 x;
  asm volatile("csrr %0, mie" : "=r"(x));
  return x;
}
static inline void w_mie(uint64 x) { asm volatile("csrw mie, %0" : : "r"(x)); }

/* 监管态异常程序计数器 sepc：记录发生Trap时的PC地址 */
static inline uint64 r_sepc() {
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r"(x));
  return x;
}
static inline void w_sepc(uint64 x) {
  asm volatile("csrw sepc, %0" : : "r"(x));
}

/* 机器态"委托"寄存器：把哪些中断/异常委托给S态处理 */
static inline uint64 r_medeleg() {
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r"(x));
  return x;
}
static inline void w_medeleg(uint64 x) {
  asm volatile("csrw medeleg, %0" : : "r"(x));
}
static inline uint64 r_mideleg() {
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r"(x));
  return x;
}
static inline void w_mideleg(uint64 x) {
  asm volatile("csrw mideleg, %0" : : "r"(x));
}

/* 监管态陷入向量寄存器 stvec：存放陷阱处理入口地址 */
static inline uint64 r_stvec() {
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r"(x));
  return x;
}
static inline void w_stvec(uint64 x) {
  asm volatile("csrw stvec, %0" : : "r"(x));
}

/* 机器态陷入向量寄存器 mtvec */
static inline void w_mtvec(uint64 x) {
  asm volatile("csrw mtvec, %0" : : "r"(x));
}

/* 地址转换与保护寄存器 satp：控制MMU分页模式和根页表地址（Lab3）*/
static inline uint64 r_satp() {
  uint64 x;
  asm volatile("csrr %0, satp" : "=r"(x));
  return x;
}
static inline void w_satp(uint64 x) {
  asm volatile("csrw satp, %0" : : "r"(x));
}

/* 监管态中断/异常原因寄存器 scause */
static inline uint64 r_scause() {
  uint64 x;
  asm volatile("csrr %0, scause" : "=r"(x));
  return x;
}

/* 监管态陷入值寄存器 stval：记录触发异常时的"出事"地址 */
static inline uint64 r_stval() {
  uint64 x;
  asm volatile("csrr %0, stval" : "=r"(x));
  return x;
}

/* 机器态硬件线程ID mhartid：获取当前CPU核心编号 */
static inline uint64 r_mhartid() {
  uint64 x;
  asm volatile("csrr %0, mhartid" : "=r"(x));
  return x;
}

/* 机器态暂存寄存器 mscratch：M-Mode 临时存储，timervec 用来找 timer_scratch */
static inline void w_mscratch(uint64 x) {
  asm volatile("csrw mscratch, %0" : : "r"(x));
}

/* 线程指针寄存器 tp：在本实验框架中用来存当前 hartid，方便 mycpu() 使用 */
static inline uint64 r_tp() {
  uint64 x;
  asm volatile("mv %0, tp" : "=r"(x));
  return x;
}
static inline void w_tp(uint64 x) { asm volatile("mv tp, %0" : : "r"(x)); }

/* 程序计数器 ra（返回地址）*/
static inline uint64 r_ra() {
  uint64 x;
  asm volatile("mv %0, ra" : "=r"(x));
  return x;
}

/* 栈指针 sp */
static inline uint64 r_sp() {
  uint64 x;
  asm volatile("mv %0, sp" : "=r"(x));
  return x;
}

/* TLB 刷新指令：修改页表后必须执行，否则CPU会使用过时的翻译缓存 */
static inline void sfence_vma() { asm volatile("sfence.vma zero, zero"); }

/* 开关全局中断的便捷函数（Lab4 使用）*/
static inline void intr_on() { w_sstatus(r_sstatus() | SSTATUS_SIE); }
static inline void intr_off() { w_sstatus(r_sstatus() & ~SSTATUS_SIE); }
static inline int intr_get() {
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

#endif /* RISCV_H */
