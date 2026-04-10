/* memlayout.h — 内存与设备地址布局（已提供，无需修改）*/
#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

/*
 * QEMU virt 平台物理内存布局（从低到高）：
 *
 * 0x00000000 -- 0x0FFFFFFF  : QEMU主板外设MMIO区域
 * 0x10000000                : UART0 串口控制器基地址
 * 0x10001000                : virtio磁盘接口基地址
 * 0x0C000000                : PLIC 中断控制器基地址
 * 0x02000000                : CLINT 计时器基地址
 * 0x80000000                : 物理内存起始地址（内核加载于此）
 * 0x88000000                : 物理内存上限（PHYSTOP，128MB）
 */

/* 关键物理地址常量 */
#define UART0 0x10000000L /* UART串口基地址（MMIO） */
#define UART0_IRQ 10      /* UART中断号 */

#define VIRTIO0 0x10001000 /* virtio磁盘基地址 */
#define VIRTIO0_IRQ 1      /* virtio中断号 */

#define PLIC 0x0c000000L /* PLIC中断控制器基地址 */
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart) * 0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart) * 0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart) * 0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart) * 0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart) * 0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart) * 0x2000)

#define CLINT 0x2000000L /* CLINT计时器基地址 */
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8 * (hartid))
#define CLINT_MTIME (CLINT + 0xBFF8)

/* 物理内存范围 */
#define KERNBASE 0x80000000L                   /* 内核加载起始地址 */
#define PHYSTOP (KERNBASE + 128 * 1024 * 1024) /* 物理内存上限 128MB */

/* 内核虚拟地址空间布局（开启虚拟内存后使用，Lab3涉及） */
#define TRAMPOLINE (MAXVA - PGSIZE)     /* 跳板页（最高虚拟页）*/
#define TRAPFRAME (TRAMPOLINE - PGSIZE) /* 陷阱帧页 */

/* Sv39 虚拟地址上限 */
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

/* 页面大小 */
#define PGSIZE 4096 /* 一页 = 4096 字节 */
#define PGSHIFT 12  /* PGSIZE 的位数 */

/* 地址对齐操作宏 */
#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))

#endif /* MEMLAYOUT_H */
