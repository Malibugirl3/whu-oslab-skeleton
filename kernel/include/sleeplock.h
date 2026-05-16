#ifndef SLEEPLOCK_H
#define SLEEPLOCK_H

#include "spinlock.h"

struct sleeplock {
  uint locked;          /* 0 = 未持有，1 = 已持有 */
  struct spinlock lk;   /* 保护本 sleeplock 内部状态 */
  char *name;           /* 锁的名称（调试用）*/
  int  pid;             /* 当前持有该锁的进程 pid（调试用）*/
};

#endif