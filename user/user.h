#ifndef USER_H
#define USER_H

int getpid(void);
int write(int fd, char *buf, int n);
int fork(void);
int wait(int *status);
void exit(int status);

int strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strcpy(char *dst, const char *src);
void *memset(void *dst, int c, int n);
void *memmove(void *dst, const void *src, int n);
void puts(const char *s);
void print_int(int x);
void print_hex(unsigned long x);
void uprintf(const char *fmt, ...);

#endif