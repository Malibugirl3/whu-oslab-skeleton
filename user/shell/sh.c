#include "user.h"
#include "cmd.h"

int main(void) {
  char line[64];
  int n;

  write(1, "sh: ready\n", 10);

  for (;;) {
    write(1, "$ ", 2);

    memset(line, 0, sizeof(line));
    n = read(0, line, sizeof(line) - 1);
    if (n <= 0) {
      write(1, "\n", 1);
      continue;
    }

    cmd_dispatch(line);
  }

  return 0;
}
