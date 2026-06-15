#include "cmd.h"
#include "user.h"

int cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;
  write(1, "builtins: help exit\n", 20);
  write(1, "external:\n", 10);  // TODO: 之后应该写成循环输出所有外部命令
  write(1, "echo\n", 5);
  write(1, "hello\n", 6);
  return 0;
}

int cmd_exit(int argc, char **argv) {
  (void)argc;
  (void)argv;
  write(1, "sh: exit ignored for init shell\n", 32);
  return 0;
}
