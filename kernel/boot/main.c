/* main.c — 内核 C 语言主函数（Lab1 任务4，后续实验持续扩展）
 *
 * 注意：随着实验推进，你需要在 start_main() 中依次添加新模块的初始化调用。
 * 每个实验结束后，start_main() 大约会是什么样子，注释中有说明。
 */

/* =============================================================
 * Lab1 完成后，这个文件应该像这样：
 *
 *   extern void uart_puts(char *s);
 *   void start_main() {
 *       uart_puts("Hello OS from RISC-V Bare-metal!\n");
 *       while(1);
 *   }
 *
 * Lab2 完成后，扩展为调用 printf 和 clear_screen。
 * Lab3 完成后，增加 kinit(), kvmininit(), kvminithart()。
 * Lab4 完成后，增加 trapinithart(), start()（移至 start.c）。
 * Lab5 完成后，增加 procinit(), scheduler()。
 * ============================================================= */

/* 声明在 uart.c 中实现的函数（Lab2完成后改用 defs.h 统一管理）*/
// extern void uart_puts(char *s);
# include "defs.h"
# include "param.h"
# include "riscv.h"


void start_main() {
  kinit();
  kvmininit();
  kvminithart();
  trapinithart();
  uartinit();
  plicinit();
  intr_on();
  procinit();    // 初始化进程表
  virtio_disk_init(); // 初始化磁盘驱动
  fsinit(ROOTDEV); // 初始化文件系统
  userinit();    // 创建第一个进程
  scheduler();   // 开始调度（永不返回）

  while (1); /* 内核死循环，不要删除 */
}
