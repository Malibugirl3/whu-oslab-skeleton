#include "cmd.h"
#include "user.h"

#define MAXPATHLEN 64

static void make_path(char *dst, const char *cmd) {
  if (cmd[0] == '/') {
    strcpy(dst, cmd);
    return;
  }

  dst[0] = '/';
  strcpy(dst + 1, cmd);
}

void cmd_run_external(char **argv) {
  char path[MAXPATHLEN];
  int pid;
  int status;

  if ((argv[0][0] != '/' && strlen(argv[0]) >= MAXPATHLEN - 1) ||
      (argv[0][0] == '/' && strlen(argv[0]) >= MAXPATHLEN)) {
    write(1, "command too long\n", 17);
    return;
  }

  make_path(path, argv[0]);

  pid = fork();
  if (pid < 0) {
    write(1, "fork failed\n", 12);
    return;
  }

  if (pid == 0) {
    exec(path, argv);
    write(1, "exec failed: ", 13);
    write(1, path, strlen(path));
    write(1, "\n", 1);
    exit(1);
  }

  wait(&status);
}
