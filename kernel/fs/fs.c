/* fs.c — 文件系统核心（Lab7 任务2-3）
 *
 * 实现"文件系统层"，在块设备缓冲层之上提供：
 *   - bmap()        : 文件逻辑块号 → 磁盘物理块号（处理直接/间接索引）
 *   - readi()       : 从 inode 文件读取数据
 *   - writei()      : 向 inode 文件写入数据
 *   - dirlookup()   : 在目录中按文件名查找 inode
 *
 * 重要概念：
 *   Inode（索引节点）= 文件的"灵魂"，存储文件大小、数据块位置等元信息
 *   Dirent（目录项）= 目录文件的内容，格式：{inum(2字节), name(14字节)}
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"

/* 文件系统布局参数 */
#define BSIZE 1024                       /* 磁盘块大小（字节）*/
#define NDIRECT 12                       /* 直接块指针数量 */
#define NINDIRECT (BSIZE / sizeof(uint)) /* 一级间接块中的指针数量 */
#define DIRSIZ 14                        /* 目录项中文件名的最大长度 */

/* 磁盘上的 inode 结构（存储在磁盘上的格式）*/
struct dinode {
  short type;  /* 文件类型（0=空闲, 1=普通文件, 2=目录, 3=设备）*/
  short major; /* 设备主号（仅 type==3 时有效）*/
  short minor; /* 设备次号（仅 type==3 时有效）*/
  short nlink; /* 硬链接计数 */
  uint size;   /* 文件大小（字节数）*/
  uint addrs[NDIRECT +
             1]; /* 数据块地址：前 NDIRECT 个是直接，最后一个是一级间接 */
};

/* 内存中的 inode（缓存版，包含磁盘版本和额外运行时信息）*/
struct inode {
  uint dev;  /* 设备号 */
  uint inum; /* inode 编号 */
  int ref;   /* 引用计数 */
  int valid; /* 内容是否从磁盘读入 */
  /* 以下字段来自磁盘 dinode（读入后缓存在这里）*/
  short type;
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT + 1];
};

/* 目录项结构（目录文件中每一条记录的格式）*/
struct dirent {
  ushort inum;       /* 该条目对应的 inode 编号（0 表示空洞/已删除）*/
  char name[DIRSIZ]; /* 文件名（最多 14 字符，不含 '\0'）*/
};

/* 声明外部函数（由其他模块提供）*/
extern struct buf *bread(uint dev, uint blockno);
extern void brelse(struct buf *b);
extern void bwrite(struct buf *b);

/* ================================================================
 * bmap — 将文件的逻辑块号映射到磁盘的物理块号
 *
 * 参数：
 *   ip — inode 指针
 *   bn — 文件内逻辑块编号（从 0 开始）
 *
 * 返回：对应的磁盘物理块号
 *
 * 映射规则（两层结构）：
 *   bn < NDIRECT     → 直接映射：ip->addrs[bn]
 *   bn < NINDIRECT   → 间接映射：读取间接块，在其中查找 addrs[bn-NDIRECT]
 *   否则             → 文件过大，panic
 *
 * 若目标块尚未分配（地址为0），自动调用 balloc() 分配新块。
 * ================================================================ */
static uint bmap(struct inode *ip, uint bn) {
  uint addr;
  struct buf *bp;
  uint *a;

  /* 直接映射（前 NDIRECT 个逻辑块）*/
  if (bn < NDIRECT) {
    if ((addr = ip->addrs[bn]) == 0) {
      /* ================================================================
       * TODO [Lab7-任务2-步骤1]：
       *   这个块尚未分配，调用 balloc 分配一个新磁盘块并将其块号写入 ip->addrs[bn]。
       * ================================================================ */
    }
    return addr;
  }

  bn -= NDIRECT;

  /* 一级间接映射 */
  if (bn < NINDIRECT) {
    /* 先确保间接指针块本身已分配 */
    if ((addr = ip->addrs[NDIRECT]) == 0) {
      /* ================================================================
       * TODO [Lab7-任务2-步骤2]：
       *   分配间接指针块本身：同样调用 balloc，将块号写入 ip->addrs[NDIRECT]。
       * ================================================================ */
    }

    /* 读取间接指针块（它里面存的是一堆物理块地址）*/
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;

    /* 在间接块中查找第 bn 个物理块地址 */
    if ((addr = a[bn]) == 0) {
      /* ================================================================
       * TODO [Lab7-任务2-步骤3]：
       *   分配实际数据块，将其块号写入间接块并将间接块写回磁盘。
       *   注意：修改间接块后必须显式调用 bwrite(bp) 将其刷回磁盘！
       * ================================================================ */
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

/* ================================================================
 * readi — 从 inode 文件中读取数据
 *
 * 参数：
 *   ip       — 要读取的 inode
 *   user_dst — 目标地址是否是用户虚拟地址（0=内核地址，1=用户地址）
 *   dst      — 目标缓冲区地址
 *   off      — 文件内偏移（字节）
 *   n        — 要读取的字节数
 *
 * 返回：实际读取的字节数（若 off 超过文件末尾则返回 0）
 * ================================================================ */
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  if (off > ip->size || off + n < off)
    return 0;
  if (off + n > ip->size)
    n = ip->size - off;

  for (tot = 0; tot < n; tot += m, off += m, dst += m) {
    /* 找到当前偏移所在的物理磁盘块 */
    uint blockno = bmap(ip, off / BSIZE);
    bp = bread(ip->dev, blockno);

    /* 计算本次从这一块可以读多少字节 */
    m = BSIZE - off % BSIZE;
    if (m > n - tot)
      m = n - tot;

    /* ================================================================
     * TODO [Lab7-任务2-步骤4]：
     *   将 bp->data 中从 (off % BSIZE) 开始的 m 字节拷贝到目标地址 dst。
     *   简化版：memcpy((void*)dst, bp->data + off % BSIZE, m)。
     *   完整版需应对用户/内核地址空间差异，使用 copyout()。
     * ================================================================ */

    brelse(bp);
  }

  return (int)tot;
}

/* ================================================================
 * dirlookup — 在目录 inode 中按文件名查找子条目
 *
 * 参数：
 *   dp   — 目录的 inode 指针（它的数据是一系列 struct dirent）
 *   name — 要查找的文件名
 *   poff — （可选输出）找到该条目在目录文件中的字节偏移
 *
 * 返回：找到则返回对应 inode 的指针（调用 iget）；未找到返回 0。
 *
 * 原理：目录也是文件！它的"文件内容"就是一条条 dirent 记录。
 * ================================================================ */
struct inode *dirlookup(struct inode *dp, char *name, uint *poff) {
  uint off, inum;
  struct dirent de;

  /* 逐条读取目录中的 dirent 记录 */
  for (off = 0; off < dp->size; off += sizeof(de)) {
    /* 从目录文件中读取一条记录 */
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup: read error");

    /* inum==0 表示这个槽位是空的（文件已删除），跳过 */
    if (de.inum == 0)
      continue;

    /* ================================================================
     * TODO [Lab7-任务3]：
     *   比较 de.name 和 name 是否相同（最多比较 DIRSIZ 个字符）。
     *   若匹配：记录偏移到 *poff，调用 iget 获取并返回 inode。
     *   注意：需要自己实现 strncmp（裸机无标准库）。
     * ================================================================ */
    (void)inum; /* 删除这行占位符 */
  }

  return 0; /* 未找到 */
}
