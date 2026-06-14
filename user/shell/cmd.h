#ifndef CMD_H
#define CMD_H

void cmd_dispatch(char *line);
int cmd_help(int argc, char **argv);
int cmd_exit(int argc, char **argv);
void cmd_run_external(char **argv);

#endif
