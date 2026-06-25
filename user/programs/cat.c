#include "user.h"

static void cat_fd(int fd) {
  char buf[128];
  int n;

  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    write(1, buf, n);
  }

  if (n < 0)
    uprintf("cat: read error\n");
}

static void cat_file(const char *path) {
  int fd;

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    uprintf("cat: cannot open %s\n", path);
    return;
  }

  cat_fd(fd);
  close(fd);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    /* 无参数：从 stdin 读（支持 cat < file 和 ls | cat） */
    cat_fd(0);
    exit(0);
  }

  for (int i = 1; i < argc; i++)
    cat_file(argv[i]);

  exit(0);
  return 0;
}
