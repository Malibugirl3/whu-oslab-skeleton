#include "user.h"

int main(void) {
  int status = -1;
  int cpid = fork();

  if (cpid < 0) {
    write(1, "fork failed\n", 12);
    exit(1);
  }

  if (cpid == 0) {
    write(1, "Child: hello\n", 13);
    exit(1);
  } else {
    int pid = wait(&status);

    if (pid > 0) {
      write(1, "Parent: child exited\n", 21);
    }
    
    while(1);

  }
}