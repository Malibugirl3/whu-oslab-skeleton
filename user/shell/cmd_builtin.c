#include "cmd.h"
#include "user.h"

int cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;
  write(1, "builtins: help exit cd\n", 23);
  write(1, "external: hello echo cat touch rm mkdir ls pwd\n", 47);
  write(1, "shell features: | > >> < &\n", 27);
  return 0;
}

int cmd_exit(int argc, char **argv) {
  (void)argc;
  (void)argv;
  write(1, "sh: exit ignored for init shell\n", 32);
  return 0;
}

int cmd_cd(int argc, char **argv) {
  char *path;

  if (argc < 2)
    path = "/";
  else
    path = argv[1];

  if (chdir(path) < 0) {
    uprintf("cd: cannot cd %s\n", path);
    return -1;
  }

  return 0;
}
