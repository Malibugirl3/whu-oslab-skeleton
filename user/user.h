#ifndef USER_H
#define USER_H

int getpid(void);
int write(int fd, char *buf, int n);
void exit(int status);

#endif