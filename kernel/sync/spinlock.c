#include "defs.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"

void initlock(struct spinlock *lk, char *name) {
    lk->name   = name;
    lk->locked = 0;
    lk->cpu    = 0;
}

int holding(struct spinlock *lk) {
    return (lk->locked && lk->cpu == mycpu());
}

void push_off(void) {
    int old = intr_get();   // 记录当前中断状态
    intr_off();             // 关中断
    if (mycpu()->noff == 0)
      mycpu()->intena = old; // 只记录第一次关中断前的状态
    mycpu()->noff++;
}
  
void pop_off(void) {
    struct cpu *c = mycpu();
    c->noff--;
    if (c->noff == 0 && c->intena)
      intr_on();            // 只有完全退出锁嵌套且之前是开的才恢复
}

void acquire(struct spinlock *lk) {
    push_off();             // 关中断，防止持锁时被中断打断
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
      ;                     // 自旋等待（原子 test-and-set）
    __sync_synchronize();   // 内存屏障，防止编译器/CPU 乱序
    lk->cpu = mycpu();
}

void release(struct spinlock *lk) {
    lk->cpu = 0;
    __sync_synchronize();   // 内存屏障
    __sync_lock_release(&lk->locked);  // 原子清零
    pop_off();              // 恢复中断
}