#include "cmd.h"
#include "user.h"

int cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;
  write(1, "builtins: help exit\n", 20);
  write(1, "external: hello\n", 16);
  return 0;
}

int cmd_exit(int argc, char **argv) {
  (void)argc;
  (void)argv;
  write(1, "sh: exit ignored for init shell\n", 32);
  return 0;
}
