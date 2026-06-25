#ifndef CMD_H
#define CMD_H

#include "userabi.h"

#define MAXLINE 256
#define MAXCMDS 8

struct cmd_info {
  char *argv[MAXARG];
  int argc;
  char *redir_in;
  char *redir_out;
  int redir_append;
};

struct pipeline {
  struct cmd_info cmds[MAXCMDS];
  int ncmds;
  int background;
};

void cmd_dispatch(char *line);
void cmd_run_pipeline(struct pipeline *pl);
int cmd_help(int argc, char **argv);
int cmd_exit(int argc, char **argv);
int cmd_cd(int argc, char **argv);
void cmd_exec_child(char **argv);

#endif
