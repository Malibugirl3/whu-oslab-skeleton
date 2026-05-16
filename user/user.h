#ifndef USER_H
#define USER_H

int getpid(void);
int write(int fd, char *buf, int n);
int fork(void);
int wait(int *status);
void exit(int status);

#endif