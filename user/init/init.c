#include "user.h"

int main(void) {
  char *argv[] = { "sh", 0 };

  write(1, "init: exec /sh\n", 15);
  exec("/sh", argv);
  write(1, "init: exec /sh failed\n", 22);

  for (;;)
    ;

  return 0;
}
