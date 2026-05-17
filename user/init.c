#include "user.h"

int main(void) {
  int status = 0;
  int pid_process = fork();

  int mypid = getpid();
  uprintf("My pid: %d\n", mypid);

  if (pid_process < 0) {
    uprintf("fork failed in init\n");
    for (;;)
      ;
  }

  if (pid_process == 0) {
    int child = fork();
    if (child < 0) {
      uprintf("fork failed in tester\n");
      exit(1);
    }
    if (child == 0) {
      uprintf("Child: hello\n");
      exit(1);
    } else {
      int dead = wait(&status);
      if (dead > 0) {
        uprintf("Parent: child %d exited with %d\n", dead, status);
      }
      exit(0);
    }
  } else {
    int dead = wait(&status);
    if (dead > 0) {
      uprintf("Process %d exited with status %d\n", dead, status);
    }
  }

  for (;;)
    ;
}