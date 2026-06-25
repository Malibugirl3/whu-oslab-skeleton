#include "cmd.h"
#include "user.h"

#define MAXPATHLEN 64

static int is_blank(char c) {
  return c == ' ' || c == '\t';
}

static void trim_line(char *s) {
  int n = strlen(s);

  while (n > 0 &&
         (s[n - 1] == '\n' || s[n - 1] == '\r' ||
          s[n - 1] == ' ' || s[n - 1] == '\t')) {
    s[n - 1] = 0;
    n--;
  }
}

static int needs_pipeline(char *line) {
  for (int i = 0; line[i]; i++) {
    if (line[i] == '|' || line[i] == '>' || line[i] == '<' || line[i] == '&')
      return 1;
  }
  return 0;
}

// 解析命令行，将命令行分割成多个参数，并存储到argv数组中
static int parse_line(char *line, char **argv) {
  int argc = 0;
  char *p = line;

  trim_line(line);

  while (*p && argc < MAXARG - 1) {
    while (is_blank(*p))
      p++;
    if (*p == 0)
      break;

    argv[argc++] = p;

    while (*p && !is_blank(*p))
      p++;
    if (*p) {
      *p = 0;
      p++;
    }
  }

  argv[argc] = 0;
  return argc;
}

static int parse_cmd(char *seg, struct cmd_info *cmd) {
  char *p = seg;

  cmd->argc = 0;
  cmd->redir_in = 0;
  cmd->redir_out = 0;
  cmd->redir_append = 0;

  trim_line(seg);
  while (*p) {
    while (is_blank(*p))
      p++;
    if (*p == 0)
      break;

    if (*p == '>') {
      p++;
      if (*p == '>') {
        cmd->redir_append = 1;
        p++;
      }
      while (is_blank(*p))
        p++;
      cmd->redir_out = p;
      while (*p && !is_blank(*p))
        p++;
      if (*p) {
        *p = 0;
        p++;
      }
      continue;
    }

    if (*p == '<') {
      p++;
      while (is_blank(*p))
        p++;
      cmd->redir_in = p;
      while (*p && !is_blank(*p))
        p++;
      if (*p) {
        *p = 0;
        p++;
      }
      continue;
    }

    cmd->argv[cmd->argc++] = p;
    while (*p && !is_blank(*p) && *p != '>' && *p != '<')
      p++;
    if (*p) {
      *p = 0;
      p++;
    }
  }

  cmd->argv[cmd->argc] = 0;
  return cmd->argc;
}

// 解析管道，将管道分割成多个命令，并存储到pipeline结构体中
static int parse_pipeline(char *line, struct pipeline *pl) {
  char *p = line;
  char *start = line;
  int n = strlen(line);

  pl->ncmds = 0;
  pl->background = 0;

  trim_line(line);
  n = strlen(line);
  if (n > 0 && line[n - 1] == '&') {
    pl->background = 1;
    line[n - 1] = 0;
    trim_line(line);
  }

  while (*p) {
    if (*p == '|') {
      *p = 0;
      if (parse_cmd(start, &pl->cmds[pl->ncmds]) > 0)
        pl->ncmds++;
      start = p + 1;
    }
    p++;
  }

  if (parse_cmd(start, &pl->cmds[pl->ncmds]) > 0)
    pl->ncmds++;

  return pl->ncmds;
}

static struct command {
  char *name;
  int (*handler)(int argc, char **argv);
} commands[] = {
  { "help", cmd_help },
  { "exit", cmd_exit },
  { "cd", cmd_cd },
  { 0, 0 },
};

// 运行内置命令
static int run_builtin(int argc, char **argv) {
  for (int i = 0; commands[i].name; i++) {
    if (strcmp(argv[0], commands[i].name) == 0)
      return commands[i].handler(argc, argv);
  }
  return -1;
}

static void run_external(int argc, char **argv) {
  int pid, status;

  pid = fork();
  if (pid < 0) {
    write(1, "fork failed\n", 12);
    return;
  }
  if (pid == 0)
    cmd_exec_child(argv);
  wait(&status);
}

void cmd_dispatch(char *line) {
  char *argv[MAXARG];
  int argc;
  struct pipeline pl;

  trim_line(line);
  if (line[0] == 0)
    return;

  if (!needs_pipeline(line)) {
    argc = parse_line(line, argv);
    if (argc == 0)
      return;
    if (run_builtin(argc, argv) == 0)
      return;
    run_external(argc, argv);
    return;
  }

  // 解析管道
  if (parse_pipeline(line, &pl) <= 0)
    return;

  cmd_run_pipeline(&pl);
}
