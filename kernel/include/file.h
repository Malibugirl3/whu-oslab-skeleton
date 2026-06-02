/* file.h — 文件系统数据结构定义 */
#ifndef FILE_H
#define FILE_H

#include "types.h"
#include "param.h"

/* inode 类型 */
#define T_FILE   1
#define T_DIR    2
#define T_DEVICE 3

/* 文件描述符类型 */
#define FD_NONE   0
#define FD_INODE  1
#define FD_DEVICE 2

/* 磁盘 inode 结构（磁盘上的持久化格式）*/
struct dinode {
    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

/* 内存 inode 结构 */
struct inode {
    uint dev;
    uint inum;
    int ref;
    int valid;
    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

/* 目录项 */
struct dirent {
    ushort inum;
    char name[DIRSIZ];
};

/* 超级块 */
struct superblock {
    uint magic;
    uint size;
    uint nblocks;
    uint ninodes;
    uint nlog;
    uint logstart;
    uint inodestart;
    uint bmapstart;
};

/* 每个打开文件的内核描述 */
struct file {
    int type;
    int ref;
    char readable;
    char writable;
    struct inode *ip;
    uint off;
};

#endif
