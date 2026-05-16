#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "types.h"

struct cpu;

struct spinlock {
  uint locked;       /* 0 = 未持有，1 = 已持有 */
  char *name;        /* 锁的名称（调试用）*/
  struct cpu *cpu;   /* 当前持有该锁的 CPU（调试用）*/
};

#endif