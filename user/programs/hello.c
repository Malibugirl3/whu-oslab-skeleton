#include "user.h"

int main(int argc, char **argv) {
  uprintf("hello: argc=%d\n", argc);
  for (int i = 0; i < argc; i++)
    uprintf("argv[%d]=%s\n", i, argv[i]);
  exit(0);
  return 0;
}
