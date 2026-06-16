#ifndef FSABI_H
#define FSABI_H

#define DIRSIZ 14

#define T_FILE      1
#define T_DIR       2
#define T_DEVICE    3


struct dirent {
  unsigned short inum;
  char name[DIRSIZ];
};

struct stat {
    int dev;
    unsigned int ino;
    short type;
    short nlink;
    unsigned int size;
};

#endif