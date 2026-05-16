#include "defs.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "types.h"

void initsleeplock(struct sleeplock *lk, char *name) {
  initlock(&lk->lk, "sleep lock");
  lk->name   = name;
  lk->locked = 0;
  lk->pid    = 0;
}

void acquiresleep(struct sleeplock *lk) {
  acquire(&lk->lk);
  while (lk->locked) {
    sleep(lk, &lk->lk);   /* 睡在 lk 上，同時釋放 lk->lk */
  }
  lk->locked = 1;
  lk->pid    = myproc()->pid;
  release(&lk->lk);
}

void releasesleep(struct sleeplock *lk) {
  acquire(&lk->lk);
  lk->locked = 0;
  lk->pid    = 0;
  wakeup(lk);              /* 喚醒所有等在 lk 上的進程 */
  release(&lk->lk);
}

int holdingsleep(struct sleeplock *lk) {
  int r;
  acquire(&lk->lk);
  r = lk->locked && (lk->pid == myproc()->pid);
  release(&lk->lk);
  return r;
}