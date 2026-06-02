/* param.h — 系统级参数常量（已提供，无需修改）*/
#ifndef PARAM_H
#define PARAM_H

#define NPROC 64                  /* 最多同时存在的进程数 */
#define NCPU 1                    /* 本实验只考虑单核 CPU */
#define NOFILE 16                 /* 每个进程最多同时打开的文件数 */
#define NFILE 100                 /* 整个内核最多同时打开的文件总数 */
#define NINODE 50                 /* 内存中最多缓存的 inode 数 */
#define NDEV 10                   /* 设备号上限 */
#define ROOTDEV 1                 /* 根文件系统所在设备号 */
#define MAXARG 32                 /* 传给 exec 的参数个数上限 */
#define MAXOPBLOCKS 10            /* 一次文件系统事务最多写入的块数 */
#define LOGSIZE (MAXOPBLOCKS * 3) /* 日志区块数 */
#define NBUF (MAXOPBLOCKS * 3)    /* Buffer Cache 槽位数 */
#define FSSIZE 1000               /* 文件系统大小（块数） */
#define MAXPATH 128               /* 文件路径字符串最大长度 */
#define BSIZE 1024                /* 磁盘块大小（字节）*/
#define DIRSIZ 14                 /* 目录项文件名最大长度 */
#define NDIRECT 12                /* 直接块指针数量 */
#define FSMAGIC 0x10203040        /* 文件系统魔法数 */
#define ROOTINO 1                 /* 根目录 inode 编号 */

#endif /* PARAM_H */
