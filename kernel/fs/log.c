/* log.c — 最小写前日志层（Lab7 进阶）
 *
 * 目标：把一次文件系统修改包装成一个简单事务。
 *
 * 磁盘布局：
 *   log.start              : 日志头块，保存本次事务包含哪些真实块号
 *   log.start + 1 ...      : 日志数据块，保存真实块的新内容副本
 *
 * 当前实现是教学版单事务模型：
 *   begin_op()  开始事务
 *   log_write() 记录一个被修改的 buf
 *   end_op()    提交、安装日志、清空日志头
 */

#include "defs.h"
#include "file.h"
#include "buf.h"
#include "param.h"
#include "spinlock.h"
#include "types.h"

#define LOG_MAX_BLOCKS (LOGSIZE - 1)  /* 1 个日志头块，其余块保存数据 */

struct logheader {
  int n;
  uint block[LOG_MAX_BLOCKS];
};

static struct log {
  struct spinlock lock;
  int dev;
  int start;
  int size;
  int outstanding;
  int committing;  // 是否正在提交事务
  struct logheader lh;
} log;

extern struct superblock sb;

static void read_head(void)
{
  struct buf *bp = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *)bp->data;

  log.lh.n = lh->n;
  for (int i = 0; i < log.lh.n; i++)
    log.lh.block[i] = lh->block[i];

  brelse(bp);
}

static void write_head(void)
{
  struct buf *bp = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *)bp->data;

  memset(bp->data, 0, BSIZE);
  lh->n = log.lh.n;
  for (int i = 0; i < log.lh.n; i++)
    lh->block[i] = log.lh.block[i];

  bwrite(bp);
  brelse(bp);
}

static void write_log(void)
{
  for (int i = 0; i < log.lh.n; i++) {
    struct buf *to = bread(log.dev, log.start + i + 1);
    struct buf *from = bread(log.dev, log.lh.block[i]);

    memmove(to->data, from->data, BSIZE);
    bwrite(to);

    brelse(from);
    brelse(to);
  }
}

static void install_trans(void)
{
  for (int i = 0; i < log.lh.n; i++) {
    struct buf *from = bread(log.dev, log.start + i + 1);
    struct buf *to = bread(log.dev, log.lh.block[i]);

    memmove(to->data, from->data, BSIZE);
    bwrite(to);

    brelse(to);
    brelse(from);
  }
}

static void commit(void)
{
  if (log.lh.n > 0) {
    write_log();      /* 1. 先把新内容写进日志数据块 */
    write_head();     /* 2. 再写日志头，表示事务已提交 */
    install_trans();  /* 3. 把日志内容安装到真实磁盘块 */
    log.lh.n = 0;
    write_head();     /* 4. 清空日志头，表示事务完成 */
  }
}

void recover_from_log(void)
{
  read_head();
  install_trans();
  log.lh.n = 0;
  write_head();
}

void loginit(int dev)
{
  if (sb.nlog < 2)
    panic("loginit: log too small");
  if (sb.nlog - 1 > LOG_MAX_BLOCKS)
    panic("loginit: log too large");

  initlock(&log.lock, "log");
  log.dev = dev;
  log.start = sb.logstart;
  log.size = sb.nlog;
  log.outstanding = 0;
  log.committing = 0;
  log.lh.n = 0;

  recover_from_log();
}

void begin_op(void)
{
  acquire(&log.lock);
  if (log.committing)
    panic("begin_op: committing");
  if (log.outstanding != 0)
    panic("begin_op: nested transaction");
  log.outstanding = 1;
  release(&log.lock);
}

void end_op(void)
{
  acquire(&log.lock);
  if (log.outstanding != 1)
    panic("end_op: no transaction");
  log.committing = 1;
  release(&log.lock);

  commit();

  acquire(&log.lock);
  log.outstanding = 0;
  log.committing = 0;
  release(&log.lock);
}

void log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.outstanding < 1)
    panic("log_write: outside of transaction");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)
      break;
  }

  if (i == log.lh.n) {
    if (log.lh.n >= log.size - 1 || log.lh.n >= LOG_MAX_BLOCKS)
      panic("log_write: too big");
    log.lh.block[log.lh.n++] = b->blockno;
  }
  release(&log.lock);

  /* 把当前块的新内容保存到对应日志数据块。 */
  struct buf *logbuf = bread(b->dev, log.start + i + 1);
  memmove(logbuf->data, b->data, BSIZE);
  bwrite(logbuf);
  brelse(logbuf);
}
