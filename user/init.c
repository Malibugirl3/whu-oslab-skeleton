#include "user.h"

int main(void) {
  write(1, "Hello from user!\n", 17);

  if (getpid() == 1)
    write(1, "getpid ok\n", 10);

  exit(0);

  for (;;)
    ;
}