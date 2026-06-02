/* vm.c — 虚拟内存与页表管理（Lab3 任务2-4）
 *
 * 本文件实现 RISC-V Sv39 三级分页机制：
 *   - walk()      : 在三级页表中查找（并按需分配中间级页表）
 *   - mappages()  : 批量建立虚拟地址到物理地址的映射
 *   - kvmininit() : 建立内核页表（映射内核各段和设备）
 *   - kvminithart(): 将页表写入 satp 寄存器，开启 MMU 分页
 *
 * 重要概念：
 *   虚拟地址（VA）→ MMU查页表 → 物理地址（PA）
 *    MMU：内存管理单元，负责虚拟地址到物理地址的映射。
 *    三级页表：Sv39 三级页表（level-2、level-1、level-0）
 *   Sv39中VA分解：[38:30]=VPN[2], [29:21]=VPN[1], [20:12]=VPN[0],
 * [11:0]=页内偏移
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

/* 内核根页表（全局变量，Lab3 建立后整个内核都使用它）*/
pagetable_t kernel_pagetable;

/* 外部符号（由链接脚本/编译器生成，标记内核各段边界）*/
extern char etext[];       /* 内核代码段结束地址 */
extern char end_address[]; /* 内核数据段结束地址 */

/* ================================================================
 * walk — 在三级页表中查找虚拟地址 va 对应的最终 PTE 指针
 *
 * 参数：
 *   pagetable — 根页表物理地址
 *   va        — 要查找的虚拟地址
 *   alloc     — 若中间级页表不存在：1=自动分配新页表，0=直接返回0
 *
 * 返回值：最底层（level-0）PTE 的指针；找不到时返回 0。
 *
 * Sv39 三级页表遍历（从 level-2 到 level-0）：
 *   每级用 PX(level, va) 提取 9 位索引，乘以8字节，找到对应 PTE。
 *   PTE 中取出物理页号（PPN），转为物理地址（下一级页表基地址）。
 * ================================================================ */
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {
  if (va >= MAXVA)
    panic("walk: virtual address out of range");

  /* 从 level-2 到 level-1，共遍历两层（最后 level-0 由调用者处理）*/
  for (int level = 2; level > 0; level--) {
    /* 取当前层的 PTE 指针 */
    pte_t *pte = &pagetable[PX(level, va)];

    if (*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      /* 该 PTE 无效：中间级页表不存在 */
      if (!alloc) {
        printf("==== walk: pte not valid and alloc is 0 ====\n");
        return 0; /* 不允许分配，返回失败 */
      }

      /* 分配一个新的物理页作为下一级页表 */
      pagetable = (pagetable_t)kalloc();
      if (pagetable == 0) {
        printf("==== walk: pagetable is 0 ====\n");
        return 0; /* 内存耗尽 */
      }

      memset(pagetable, 0, PGSIZE);

      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }

  /* 返回 level-0 页表中对应的 PTE 指针（最终映射层）*/
  return &pagetable[PX(0, va)];
}

/* ================================================================
 * mappages — 建立从虚拟地址范围到物理地址范围的映射
 *
 * 参数：
 *   pagetable — 根页表
 *   pa        — 对应的物理地址起始
 *   va        — 虚拟地址起始（会被向下对齐到页边界）
 *   size      — 映射大小（字节）
 *   perm      — 权限位（PTE_R/PTE_W/PTE_X/PTE_U 的组合）
 *
 * 返回值：0 表示成功，-1 表示失败（内存不足）
 * ================================================================ */
int mappages(pagetable_t pagetable, uint64 pa, uint64 va, uint64 size,
             int perm) {
  uint64 a, last;
  pte_t *pte;

  if (size == 0)
    panic("mappages: size is 0");

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);

  for (;;) {
    /* 找到 va=a 对应的 level-0 PTE（必要时分配中间页表）*/
    if ((pte = walk(pagetable, a, 1)) == 0)
      return -1;

    /* 重复映射是内核 Bug */
    if (*pte & PTE_V)
      panic("mappages: remap");

    /* ================================================================
     * TODO [Lab3-任务3]：
     *   将物理地址 pa 和权限 perm 以及有效位 PTE_V 写入 *pte。
     *   PTE格式：高位为PPN（PA2PTE得到），低位为权限位（perm | PTE_V）。
     * ================================================================ */
    *pte = PA2PTE(pa) | perm | PTE_V;

    if (a == last)
      break;

    a += PGSIZE; 
    pa += PGSIZE;
  }
  return 0;
}

/* ================================================================
 * kvmininit — 建立内核页表
 *
 * 建立的映射关系（"恒等映射"：虚拟地址 = 物理地址，方便内核访问）：
 *   UART0   设备 → 可读写
 *   内核代码段   → 可读+可执行
 *   内核数据段   → 可读+可写
 *   可用物理内存 → 可读+可写
 *
 * 注：更完整的版本还需映射 PLIC、virtio 等设备（Lab7使用）。
 * ================================================================ */
void kvmininit(void) {
  /* 分配根页表 */
  kernel_pagetable = (pagetable_t)kalloc();
  if (kernel_pagetable == 0)
    panic("kvmininit: out of memory");

  /* 清零根页表 */
  for (int i = 0; i < PGSIZE / 8; i++)
    ((uint64 *)kernel_pagetable)[i] = 0;

  /* ================================================================
   * TODO [Lab3-任务4-步骤1]：
   *   映射 UART0 串口设备（MMIO区域），使内核可以访问串口寄存器。
   *   地址：UART0（见 memlayout.h），大小：PGSIZE，权限：可读+可写。
   * ================================================================ */
  if (mappages(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W) != 0)
    panic("kvmininit: failed to map UART0");
  if (mappages(kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W) != 0)
    panic("kvmininit: failed to map VIRTIO0");

  /* ================================================================
   * TODO [Lab3-任务4-步骤2]：
   *   映射内核代码段：从 KERNBASE 到 etext。
   *   权限：可读+可执行（注意：代码段不能有写权限！）。
   * ================================================================ */
  if (mappages(kernel_pagetable, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X) != 0)
    panic("kvmininit: failed to map kernel code");
  /* ================================================================
   * TODO [Lab3-任务4-步骤3]：
   *   映射内核数据段和剩余可用物理内存：从 etext 到 PHYSTOP。
   *   权限：可读+可写（数据段需要写权限，但不能有可执行权限）。
   * ================================================================ */
  if (mappages(kernel_pagetable, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W) != 0)
    panic("kvmininit: failed to map kernel data");

// 映射 PLIC 中断控制器（4MB，覆盖 PLIC_PRIORITY 到 PLIC_SCLAIM 全部寄存器）
if (mappages(kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W) != 0)
    panic("kvmininit: failed to map PLIC");
}

/* ================================================================
 * kvminithart — 开启当前 CPU 核心的 MMU 分页
 *
 * 将 kernel_pagetable 写入 satp 寄存器，告诉 MMU：
 *   "从现在起，所有地址访问都要经过你查页表翻译！"
 *
 * 注意：执行完这条指令后，CPU 立即开始查页表。
 *       如果 kvmininit 的映射写错了，下一条指令就会产生 Page Fault，
 *       导致系统崩溃（此时无任何错误提示，GDB 单步调试是唯一出路）。
 * ================================================================ */
void kvminithart(void) {
  /* ================================================================
   * TODO [Lab3-任务4-步骤4]：
   *   1. 将根页表地址写入 satp 寄存器（开启Sv39分页）。
   *      使用 MAKE_SATP 宏将根页表物理地址转为 satp 的格式。
   *   2. 刷新 TLB（清空CPU中缓存的旧地址翻译结果）：sfence_vma()。
   *   注意：这两步顺序不能颠倒！
   * ================================================================ */
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

static void freewalk(pagetable_t pagetable) {
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) == 0)
      continue;

    // 不是叶子项（没有 R/W/X），说明它指向下一级页表
    if ((pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else {
      // 叶子项应该在 uvmfree 的第一阶段被清掉，这里保险清零
      pagetable[i] = 0;
    }
  }

  kfree((void *)pagetable);
}

uint64 walkaddr(pagetable_t pagetable, uint64 va) {
  if (va >= MAXVA)
    return 0;

  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if ((*pte & PTE_V) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;

  uint64 pa = PTE2PA(*pte);
  return pa + (va & (PGSIZE - 1));
}

pagetable_t uvmcreate(void) {
  pagetable_t pagetable = (pagetable_t)kalloc();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);

  // 方案B下，用户态运行时也会使用 p->pagetable。
  // 因此该页表必须具备基础内核映射，保证陷阱切换路径可用。
  if (mappages(pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W) != 0)  
    goto bad;
  if (mappages(pagetable, KERNBASE, KERNBASE, (uint64)etext - KERNBASE,
               PTE_R | PTE_X) != 0)
    goto bad;
  if (mappages(pagetable, (uint64)etext, (uint64)etext,
               PHYSTOP - (uint64)etext, PTE_R | PTE_W) != 0)
    goto bad;
  if (mappages(pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W) != 0)
    goto bad;

  return pagetable;

bad:
  // 这里没有用户页，sz 传 0 即可；会释放已分配的页表页。
  uvmfree(pagetable, 0);
  return 0;
}

int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) {
  uint64 va = 0;

  for (va = 0; va < sz; va += PGSIZE) {
    pte_t *pte = walk(old, va, 0);
    if (pte == 0 || (*pte & PTE_V) == 0)
      goto err;

    uint64 pa = PTE2PA(*pte);
    uint64 flags = PTE_FLAGS(*pte) & (PTE_R | PTE_W | PTE_X | PTE_U);

    char *mem = kalloc();
    if (mem == 0)
      goto err;

    memmove(mem, (void *)pa, PGSIZE);

    // 注意你项目 mappages 参数顺序是 (pagetable, pa, va, size, perm)
    if (mappages(new, (uint64)mem, va, PGSIZE, flags) != 0) {
      kfree(mem);
      goto err;
    }
  }

  return 0;

err:
  // 回滚已复制成功的用户页，避免 fork 失败时泄漏物理页。
  for (uint64 a = 0; a < va; a += PGSIZE) {
    pte_t *pte = walk(new, a, 0);
    if (pte == 0 || (*pte & PTE_V) == 0)
      continue;
    if ((*pte & (PTE_R | PTE_W | PTE_X)) == 0)
      continue;

    uint64 pa = PTE2PA(*pte);
    kfree((void *)pa);
    *pte = 0;
  }
  return -1;
}

void uvmfree(pagetable_t pagetable, uint64 sz) {
  // 第一阶段：释放用户叶子页（物理页）
  for (uint64 va = 0; va < sz; va += PGSIZE) {
    pte_t *pte = walk(pagetable, va, 0);
    if (pte == 0)
      continue;
    if ((*pte & PTE_V) == 0)
      continue;
    if ((*pte & (PTE_R | PTE_W | PTE_X)) == 0)
      continue; // 非叶子，交给 freewalk

    uint64 pa = PTE2PA(*pte);
    kfree((void *)pa);
    *pte = 0;
  }

  // 第二阶段：递归释放页表页
  freewalk(pagetable);
}


/*
 * copyin — 从用户态页表中复制数据到内核态
 *
 * 参数：
 *   pagetable — 用户态页表
 *   dst       — 内核态目标地址
 *   srcva     — 用户态源虚拟地址
 *   len       — 复制长度
 *
 * 返回值：0 表示成功，-1 表示失败
 */
 int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
  uint64 n, va0, pa0;
  uint64 old_sstatus = r_sstatus();

  w_sstatus(old_sstatus | SSTATUS_SUM);

  while (len > 0) {
    va0 = PGROUNDDOWN(srcva);

    pte_t *pte = walk(pagetable, va0, 0); // 获取页表项
    if (pte == 0 || !(*pte & PTE_V) || !(*pte & PTE_U) || !(*pte & PTE_R)) {
      printf("copyin: bad user address %p\n", srcva);
      w_sstatus(old_sstatus);
      return -1;
    }

    pa0 = PTE2PA(*pte); // 获取物理页号

    n = PGSIZE - (srcva - va0);
    if (n > len)
      n = len;

    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }

  w_sstatus(old_sstatus);
  return 0;
}


int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len) {
  uint64 n, va0, pa0;
  uint64 old_sstatus = r_sstatus();

  w_sstatus(old_sstatus | SSTATUS_SUM);
  
  while (len > 0) {
    va0 = PGROUNDDOWN(dstva);
  
    pte_t *pte = walk(pagetable, va0, 0);
    if (pte == 0 || !(*pte & PTE_V) || !(*pte & PTE_U) || !(*pte & PTE_W)) {
      w_sstatus(old_sstatus);
      return -1;
    }
  
    pa0 = PTE2PA(*pte);
  
    n = PGSIZE - (dstva - va0);
    if (n > len)
      n = len;
  
    memmove((void *)(pa0 + (dstva - va0)), src, n);
  
    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  
  w_sstatus(old_sstatus);
  return 0;
}

/*
 * 复制字符串从用户态页表到内核态
 * 参数：
 *   pagetable：用户态页表
 *   dstva：内核态目标地址
 *   srcva：用户态源虚拟地址
 *   max：最大长度
 * 返回：
 *   0：成功
 *   -1：失败
 */
int copyinstr(pagetable_t pagetable, char *dstva, uint64 srcva, uint64 max) {
  uint64 va0, pa0;
  int got_null = 0;
  uint64 old_sstatus = r_sstatus();

  w_sstatus(old_sstatus | SSTATUS_SUM);

  while (!got_null && max > 0) {
    va0 = PGROUNDDOWN(srcva);

    pte_t *pte = walk(pagetable, va0, 0);
    if (pte == 0 || !(*pte & PTE_V) || !(*pte & PTE_U) || !(*pte & PTE_R)) {
      w_sstatus(old_sstatus);
      return -1;
    }

    pa0 = PTE2PA(*pte);

    uint64 n = PGSIZE - (srcva - va0);
    if (n > max)
      n = max;

    char *p = (char *)(pa0 + (srcva - va0));

    while (n) {
      char c = *p;
      *dstva = c;

      dstva++;
      p++;
      srcva++;
      max--;
      n--;

      if (c == '\0') {
        got_null = 1;
        break;
      }
    }
  }

  w_sstatus(old_sstatus);

  if (got_null)
    return 0;
  return -1;
}