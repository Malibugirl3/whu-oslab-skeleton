/* buf.h — 磁盘缓冲块结构定义 */
#ifndef BUF_H
#define BUF_H

#include "types.h"

/* 缓冲块结构 */
struct buf {
  int valid;         /* 数据是否有效 */
  int disk;          /* 是否正在磁盘 I/O */
  uint dev;          /* 设备号 */
  uint blockno;      /* 磁盘块号 */
  uint refcnt;       /* 引用计数 */
  struct buf *prev;  /* LRU 前驱 */
  struct buf *next;  /* LRU 后继 */
  uchar data[BSIZE]; /* 数据内容 */
};

#endif
