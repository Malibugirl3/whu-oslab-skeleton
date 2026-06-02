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
#include "buf.h"
#include "file.h"
#include "proc.h"

/* 文件系统布局参数 */
#define NDIRECT 12                       /* 直接块指针数量 */
#define NINDIRECT (BSIZE / sizeof(uint)) /* 一级间接块中的指针数量 */
#define IPB       (BSIZE / sizeof(struct dinode))
#define BPB       (BSIZE * 8)


/* 声明外部函数（由其他模块提供）*/
extern struct buf *bread(uint dev, uint blockno);
extern void brelse(struct buf *b);
extern void bwrite(struct buf *b);

/*
superblock — 文件系统超级块

参数：无

返回：超级块结构体

原理：
  超级块是文件系统的元数据，存储了文件系统的基本信息。

  包括：
    magic — 文件系统魔数
    size — 文件系统总大小
    nblocks — 数据块总数
    ninodes — inode 总数
    nlog — 日志块数
    logstart — 日志起始块号
    inodestart — inode 区起始块号
    bmapstart — bitmap 区起始块号
*/
struct superblock sb;


/* inode 缓存 — 内存中的 inode 缓存 */
struct {
  struct inode inode[NINODE];
} icache;

/*
fsinit — 初始化文件系统

参数：
  dev — 设备号

返回：无

原理：
  初始化文件系统，包括初始化缓冲池和超级块。
*/
void 
fsinit(int dev) 
{
  struct buf *bp;

  binit();

  bp = bread(dev, 1); // 读取超级块
  memmove(&sb, bp->data, sizeof(sb)); // 将超级块数据移动到内存中
  brelse(bp); // 释放超级块缓冲区

  if (sb.magic != FSMAGIC)
    panic("fsinit: invalid file system!");

  for (int i = 0;  i < NINODE; i++) {
    icache.inode[i].ref = 0;
    icache.inode[i].valid = 0;
  }
}

/*
iget — 获取 inode 指针

参数：
  dev — 设备号
  inum — inode 编号

返回：inode 指针
*/
struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  empty = 0;
  for (ip = icache.inode; ip < icache.inode + NINODE; ip++) {
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
      ip->ref++;
      return ip;
    }
    if (empty == 0 && ip->ref == 0)
      empty = ip; // 找到一个空闲的inode
  }

  if (empty == 0)
    panic("iget: no inodes");

  empty->dev = dev;
  empty->inum = inum;
  empty->ref = 1;
  empty->valid = 0;
  return empty;
}


/*
ilock — 锁定 inode

参数：
  ip — inode 指针

返回：无

原理：
  锁定 inode，如果 inode 未锁定，则从磁盘读取 inode 数据。
*/
void
ilock(struct inode *ip) 
{
  if (ip->valid == 0) {
    struct buf *bp;
    struct dinode *dip;

    uint blockno = sb.inodestart + ip->inum / IPB; // 计算inode所在的磁盘块号
    bp = bread(ip->dev, blockno);
    dip = (struct dinode*)bp->data + ip->inum % IPB;

    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));

    brelse(bp);
    ip->valid = 1;
    
    if (ip->type == 0)
      panic("ilock: no type");
  }
}

void
iunlock(struct inode *ip)
{
}

/*
iupdate — 更新 inode

参数：
  ip — inode 指针

返回：无

原理：
  更新 inode，将 inode 数据写回磁盘。
*/
void 
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  uint blockno = sb.inodestart + ip->inum / IPB;
  bp = bread(ip->dev, blockno);
  dip = (struct dinode*)bp->data + ip->inum % IPB;

  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));

  bwrite(bp);
  brelse(bp);

}

void 
iput(struct inode *ip)
{
  ip->ref--;
  if (ip->ref == 0) {
    ip->valid = 0;
  }
}

struct inode*
ialloc(uint dev, short type)
{
  struct buf *bp;
  struct dinode *dip;

  for (int inum = 1; inum < sb.ninodes; inum++) {
    uint blockno = sb.inodestart + inum / IPB;
    bp = bread(dev, blockno);
    dip = (struct dinode*)bp->data + inum % IPB;

    if (dip->type == 0) {
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      bwrite(bp);
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
}

/*
bzero — 清零一个磁盘块

参数：
  dev — 设备号
  bno — 磁盘块号

返回：无
*/
static void
bzero(uint dev, uint bno)
{
    struct buf *bp;

    bp = bread(dev, bno);
    memset(bp->data, 0, BSIZE);
    bwrite(bp);
    brelse(bp);
}

/*
balloc — 分配一个空闲磁盘块

参数：
  dev — 设备号

返回：分配的磁盘块号

原理：
  遍历每个 bitmap 块，找到第一个空闲位，将其标记为占用，并返回对应的物理数据块号。
*/
static uint
balloc(uint dev)
{
    int b, bi, m;
    struct buf *bp;

    // 外层：以 BPB(8192) 为步长，遍历每个 bitmap 块
    for (b = 0; b < sb.nblocks; b += BPB) {
        // 读出当前这个 bitmap 块
        bp = bread(dev, sb.bmapstart + b / BPB);

        // 内层：扫描 bitmap 块内的每一个 bit
        for (bi = 0; bi < BPB && b + bi < sb.nblocks; bi++) {       
            m = 1 << (bi % 8);              // 构造掩码：第几位是 1 
            if ((bp->data[bi / 8] & m) == 0) {  // 这个 bit 是      0（空闲）吗？
                bp->data[bi / 8] |= m;      // 改成 1（标记占用）   
                bwrite(bp);                 // 写回磁盘
                brelse(bp);                 // 释放 bitmap 块       
                bzero(dev, b + bi);         // 把分配的数据块清零   
                return b + bi;              // 返回物理数据块号
            }
        }
        brelse(bp);  // 这个 bitmap 块没有空闲位，释放它
    }
    panic("balloc: out of blocks");  // 整个文件系统没有空闲块了    
}

/*
bfree — 释放一个磁盘块

参数：
  dev — 设备号
  b — 磁盘块号

返回：无
*/
static void
bfree(uint dev, uint b)
{
    struct buf *bp;
    int bi, m;

    // 读出 b 号数据块对应的 bitmap 块
    bp = bread(dev, sb.bmapstart + b / BPB);

    // bi = b 在这个 bitmap 块内的第几个 bit
    bi = b % BPB;
    m = 1 << (bi % 8);           // 构造掩码

    if ((bp->data[bi / 8] & m) == 0)
        panic("bfree: freeing free block");  // 重复释放？bug！

    bp->data[bi / 8] &= ~m;      // 把对应 bit 清零
    bwrite(bp);                  // 写回磁盘
    brelse(bp);                  // 释放
}

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
       addr = balloc(ip->dev);
       ip->addrs[bn] = addr;
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
       addr = balloc(ip->dev);
       ip->addrs[NDIRECT] = addr;
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
       addr = balloc(ip->dev);
       a[bn] = addr;
       bwrite(bp);
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
    if (user_dst) {
      if (copyout(myproc()->pagetable, dst, (char*)bp->data + off % BSIZE, m) < 0) {
        brelse(bp);
        return -1;
      }
    } else {
      memcpy((void*)dst, bp->data + off % BSIZE, m);
    }

    brelse(bp);
  }

  return (int)tot;
}

/*
writei — 向 inode 文件中写入数据

参数：
  ip — inode 指针
  user_src — 源地址是否是用户虚拟地址（0=内核地址，1=用户地址）
  src — 源地址
  off — 文件内偏移（字节）
  n — 要写入的字节数
*/
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n) {
  uint tot, m, blockno;
  struct buf *bp;

  if (off + n < off) 
    return -1;
  if (off + n > ip->size) 
    ip->size = off + n;

  for (tot = 0; tot < n; tot += m, off += m, src += m) {
    blockno = bmap(ip, off / BSIZE);
    bp = bread(ip->dev, blockno);
    m = BSIZE - off % BSIZE;
    if (m > n - tot)
      m = n - tot;

    // 从 src 拷到 bp->data, 然后bwrite
    if (user_src) {
      if (copyin(myproc()->pagetable, (char*)bp->data + off % BSIZE, src, m) < 0) {
        brelse(bp);
        return -1;
      }
    } else {
      memmove((void*)(bp->data + off % BSIZE), (void*)src, m);
    }
    bwrite(bp);
    brelse(bp);
  }

  iupdate(ip);
  return n;

}



int
dirlink(struct inode *dp, char *name, uint inum)
{
    struct dirent de;
    uint off;

    // 先找空洞
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlink: read");
        if (de.inum == 0)        // 找到空洞，在这里写入
            break;
    }

    // off 到了 dp->size 说明没空洞，在末尾追加

    memset(&de, 0, sizeof(de));   // 清零，确保 padding 全 0
    de.inum = inum;
    memmove(de.name, name, DIRSIZ);

    writei(dp, 0, (uint64)&de, off, sizeof(de));

    return 0;
}

static char*
skipelem(char *path, char *name)
{
    while (*path == '/') path++;
    if (*path == 0) return 0;
    char *s = path;
    while (*path != '/' && *path != 0) path++;
    int len = path - s;
    if (len > DIRSIZ) len = DIRSIZ;
    memmove(name, s, len);
    name[len] = 0;
    while (*path == '/') path++;
    return path;
}

struct inode*
namei(char *path)
{
    char name[DIRSIZ];
    struct inode *ip, *next;

    if (*path == '/')
        ip = iget(ROOTDEV, ROOTINO);
    else
        return 0;

    while ((path = skipelem(path, name)) != 0) {
        ilock(ip);
        if (ip->type != T_DIR) {
            iunlock(ip);
            iput(ip);
            return 0;
        }
        next = dirlookup(ip, name, 0);
        iunlock(ip);
        iput(ip);
        if (next == 0)
            return 0;
        ip = next;
    }
    return ip;
}

// 返回父目录 inode，最后一段文件名写入 name
struct inode*
nameiparent(char *path, char *name)
{
    struct inode *ip, *next;

    if (*path == '/')
        ip = iget(ROOTDEV, ROOTINO);
    else
        return 0;

    while ((path = skipelem(path, name)) != 0) {
        ilock(ip);
        if (ip->type != T_DIR) {
            iunlock(ip); iput(ip); return 0;
        }
        char peek_name[DIRSIZ];
        if (skipelem(path, peek_name) == 0) {
            iunlock(ip);
            return ip;          // ip 是父目录，name 已是最后一段
        }
        next = dirlookup(ip, name, 0);
        iunlock(ip);
        iput(ip);
        if (next == 0)
            return 0;
        ip = next;
    }
    return ip;                  // ip=父目录，name=文件名
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
  uint off;
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
    if (strncmp(de.name, name, DIRSIZ) == 0) {
      if (poff) *poff = off;
      return iget(dp->dev, de.inum);
    }
  }

  return 0; /* 未找到 */
}
