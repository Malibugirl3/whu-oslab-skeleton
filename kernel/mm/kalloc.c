/* kalloc.c — 物理内存页面分配器（Lab3 任务1）
 *
 * 策略：将所有空闲的物理页面组成一个单向链表（Free List）。
 * 巧妙之处：链表节点本身就存放在空闲页面的前 8 字节中，不需要额外空间！
 *
 * 数据流向：
 *   kinit()      → 初始化：把内核结束地址到 PHYSTOP 之间的页面全部加入链表
 *   kalloc()     → 分配：从链表头取出一页，返回其地址
 *   kfree(pa)    → 回收：将地址 pa 对应的页插回链表头
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

/* 链表节点结构 —— 注意：它"住"在空闲物理页面的最前面 8 字节 */
struct free_node {
  struct free_node *next;
};

/* 空闲物理页链表的头指针（全局变量，初始为 NULL）*/
static struct free_node *free_mem_list;

/* 内核数据段的结束地址，由链接脚本 kernel.ld 定义
 * kalloc 从 end_address 之后开始管理可用内存 */
extern char end_address[];

/* ================================================================
 * kinit — 初始化物理内存分配器
 *
 * 任务：将 [end_address, PHYSTOP) 中的所有页面，逐页调用 kfree
 *       释放到 freelist 中，完成初始化。
 *
 * 提示：
 *   - PGROUNDUP(addr) 将 addr 向上对齐到下一个页边界（4096的整数倍）
 *   - PHYSTOP 在 memlayout.h 中定义，是物理内存的上限
 *   - 每次循环，p 指针前进 PGSIZE（4096）字节
 * ================================================================ */
void kinit(void) {
  /* ================================================================
   * TODO [Lab3-任务1-步骤1]：
   *   将从 end_address 到 PHYSTOP 的全部内存页释放到 free_mem_list。
   *   要求：起始地址按4KB对齐（使用 PGROUNDUP），每次步进 PGSIZE。
   * ================================================================ */
  uint64 p;  // 之所以用uint64是因为end_address是一个指针，需要转换为uint64才能进行计算

  for (p = PGROUNDUP((uint64)end_address); p < PHYSTOP; p += PGSIZE)
    kfree((void*)p);
}

/* ================================================================
 * kfree — 回收一个 4KB 物理页面到 free_mem_list
 *
 * 参数：pa — 要回收的物理页面起始地址（必须是 4096 对齐的）
 *
 * 算法（头插法）：
 *   1. 安全检查：地址必须 4KB 对齐，且在合法范围内
 *   2. 将 pa 强转为 struct free_node* 指针 r
 *   3. r->next = free_mem_list（新节点指向原来的链表头）
 *   4. free_mem_list = r（链表头更新为新节点）
 *
 * 调试技巧：可先用 memset 把页面填充为 0xAB（垃圾值），
 *           如果内核意外读取到 0xAB 开头的数据，说明用了未初始化的内存。
 * ================================================================ */
void kfree(void *pa) {
  struct free_node *r;  

  /* 安全检查（已提供，无需修改）*/
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end_address ||
      (uint64)pa >= PHYSTOP) {
    panic("kfree: invalid address");
  }

  /* 将页面内容填充为垃圾值，帮助发现"使用已释放内存"的错误 */
  /* memset(pa, 1, PGSIZE); */ /* 可选：用于调试 */

  /* ================================================================
   * TODO [Lab3-任务1-步骤2]：
   *   实现头插法，将 pa 插入 free_mem_list 链表头。
   *   将 pa 强转为 struct free_node*，使其 next 指向原链表头，再更新链表头。
   * ================================================================ */
  r = (struct free_node *)pa;
  r->next = free_mem_list;
  free_mem_list = r; 

}

/* ================================================================
 * kalloc — 分配一个 4KB 物理页面
 *
 * 返回值：分配到的页面起始地址；若内存耗尽，返回 0（NULL）。
 *
 * 算法：
 *   1. 取 free_mem_list 的链表头节点 r
 *   2. 若 r 不为空，将 free_mem_list 更新为 r->next（摘除链表头）
 *   3. 将页面内容清零（安全起见），然后返回 r， 类型为 void*
 * ================================================================ */
void *kalloc(void) {
  struct free_node *r;

  /* ================================================================
   * TODO [Lab3-任务1-步骤3]：
   *   从 free_mem_list 链表头摘出一页并返回。
   *   若链表为空（free_mem_list==0）则内存耗尽，返回0。
   *   分配成功后建议将页面内容清零，防止信息泄漏。
   * ================================================================ */
  if (free_mem_list == 0) {
    // panic("kalloc: memory exhausted");
    return 0;
  }

  r = free_mem_list;
  free_mem_list = r->next;
  // memset(r, 0, PGSIZE); 
  // uint64 *p = (uint64 *)r;
  // for (int i = 0; i < PGSIZE / sizeof(uint64); i++)
  //     p[i] = 0;
  return r;   
}
