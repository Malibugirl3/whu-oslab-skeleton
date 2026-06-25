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

void cmd_exec_child(char **argv) {
  char path[MAXPATHLEN];

  if ((argv[0][0] != '/' && strlen(argv[0]) >= MAXPATHLEN - 1) ||
      (argv[0][0] == '/' && strlen(argv[0]) >= MAXPATHLEN)) {
    write(1, "command too long\n", 17);
    exit(1);
  }

  make_path(path, argv[0]);
  exec(path, argv);
  write(1, "exec failed: ", 13);
  write(1, path, strlen(path));
  write(1, "\n", 1);
  exit(1);
}

/*
* 运行管道
* 参数：pipeline结构体
* 返回值：无
*/
void cmd_run_pipeline(struct pipeline *pl) {
  int pipes[MAXCMDS][2];
  int i, j, pid, status, fd;
  struct cmd_info *cmd;

  for (i = 0; i < pl->ncmds - 1; i++) {
    if (pipe(pipes[i]) < 0) {
      write(1, "pipe failed\n", 12);
      return;
    }
  }

  for (i = 0; i < pl->ncmds; i++) {
    cmd = &pl->cmds[i];
    pid = fork();
    if (pid < 0) {
      write(1, "fork failed\n", 12);
      return;
    }

    if (pid == 0) {
      if (i > 0) {
        dup2(pipes[i - 1][0], 0);
      } else if (cmd->redir_in) {
        fd = open(cmd->redir_in, O_RDONLY);
        if (fd < 0) {
          uprintf("sh: cannot open %s\n", cmd->redir_in);
          exit(1);
        }
        dup2(fd, 0);
        close(fd);
      }

      if (i < pl->ncmds - 1) {
        dup2(pipes[i][1], 1);
      } else if (cmd->redir_out) {
        int flags = O_WRONLY | O_CREAT;
        if (cmd->redir_append)
          flags |= O_APPEND;
        else
          flags |= O_TRUNC;
        fd = open(cmd->redir_out, flags);
        if (fd < 0) {
          uprintf("sh: cannot open %s\n", cmd->redir_out);
          exit(1);
        }
        dup2(fd, 1);
        close(fd);
      }

      for (j = 0; j < pl->ncmds - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }

      cmd_exec_child(cmd->argv);
    }
  }

  for (i = 0; i < pl->ncmds - 1; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }

  if (!pl->background) {
    for (i = 0; i < pl->ncmds; i++)
      wait(&status);
  }
}
