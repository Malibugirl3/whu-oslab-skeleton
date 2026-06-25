#ifndef USER_H
#define USER_H

#include "userabi.h"
#include "fsabi.h"

int open(const char *path, int flags);
int read(int fd, char *buf, int n);
int write(int fd, char *buf, int n);
int close(int fd);
int unlink(const char *path);
int exec(const char *path, char **argv);
int getpid(void);
int fork(void);
int wait(int *status);
void exit(int status);
int mkdir(const char *path);
int fstat(int fd, struct stat *st);
int chdir(const char *path);
int dup(int fd);
int dup2(int oldfd, int newfd);
int pipe(int fds[2]);
int stat(const char *path, struct stat *st);
int getcwd(char *buf, int size);
void *sbrk(int n);

int strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strcpy(char *dst, const char *src);
void *memset(void *dst, int c, int n);
void *memmove(void *dst, const void *src, int n);
void puts(const char *s);
void print_int(int x);
void print_hex(unsigned long x);
void print_padded(const char *s, int width);
void print_int_padded(int x, int width);
void uprintf(const char *fmt, ...);

#endif
