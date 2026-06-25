#include "user.h"

int main(void) {
  char cwd[MAXPATH];

  if (getcwd(cwd, sizeof(cwd)) < 0)
    strcpy(cwd, "/");
  write(1, cwd, strlen(cwd));
  write(1, "\n", 1);
  exit(0);
  return 0;
}
