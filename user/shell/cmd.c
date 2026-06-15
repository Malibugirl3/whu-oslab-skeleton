#include "cmd.h"
#include "user.h"

struct command {
  char *name;
  int (*handler)(int argc, char **argv);
};

static void trim_line(char *s) {
  int n = strlen(s);

  while (n > 0 &&
         (s[n - 1] == '\n' || s[n - 1] == '\r' ||
          s[n - 1] == ' ' || s[n - 1] == '\t')) {
    s[n - 1] = 0;
    n--;
  }
}

static int parse_line(char *line, char **argv) {
  int argc = 0;
  char *p = line;

  trim_line(line); // 去除行末的换行符、空格和制表符

  while (*p && argc < MAXARG - 1) {
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == 0)
      break;

    argv[argc++] = p;

    while (*p && *p != ' ' && *p != '\t')
      p++;
    if (*p) {
      *p = 0;
      p++;
    }
  }

  argv[argc] = 0;
  return argc;
}

static struct command commands[] = {
  { "help", cmd_help },
  { "exit", cmd_exit },
  { 0, 0 },
};

static int run_builtin(int argc, char **argv) {
  for (int i = 0; commands[i].name; i++) {
    if (strcmp(argv[0], commands[i].name) == 0)
      return commands[i].handler(argc, argv);
  }
  return -1;
}

void cmd_dispatch(char *line) {
  char *argv[MAXARG];
  int argc = parse_line(line, argv);

  if (argc == 0)
    return;

  if (run_builtin(argc, argv) == 0)
    return;

  cmd_run_external(argv);
}
