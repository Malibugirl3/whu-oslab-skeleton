#include "user.h"
#include "fsabi.h"

static const char *type_name(short type) {
  if (type == T_DIR)
    return "DIR";
  if (type == T_FILE)
    return "FILE";
  if (type == T_DEVICE)
    return "DEV";
  return "????";
}

static void print_padded(const char *s, int width) {
  int n = strlen(s);

  if (n > width)
    n = width;

  write(1, (char *)s, n);
  for (int i = n; i < width; i++)
    write(1, " ", 1);
}

static void print_int_padded(int x, int width) {
  char buf[16];
  int n = 0;
  int i;

  if (x == 0) {
    buf[n++] = '0';
  } else {
    if (x < 0) {
      write(1, "-", 1);
      x = -x;
      width--;
    }
    while (x > 0) {
      buf[n++] = '0' + (x % 10);
      x /= 10;
    }
    for (i = 0; i < n / 2; i++) {
      char t = buf[i];
      buf[i] = buf[n - 1 - i];
      buf[n - 1 - i] = t;
    }
  }

  write(1, buf, n);
  for (i = n; i < width; i++)
    write(1, " ", 1);
}

static void ls_file(struct stat *st, char name[DIRSIZ]) {
  print_padded(name, DIRSIZ);
  write(1, "\t", 1);
  print_padded(type_name(st->type), 5);
  write(1, "\t", 1);
  print_int_padded(st->ino, 6);
  write(1, "\t", 1);
  print_int_padded(st->size, 8);
  write(1, "\n", 1);
}

static void make_path(char *dst, const char *dir, const char *name) {
  int i = 0;

  while (dir[i]) {
    dst[i] = dir[i];
    i++;
  }

  if (i > 0 && dst[i - 1] != '/') {
    dst[i++] = '/';
  }

  int j = 0;
  while (name[j]) {
    dst[i++] = name[j++];
  }

  dst[i] = 0;
}

static void ls(char *path) {
  char fullpath[MAXPATH];
  int fd, fd2, n;
  struct dirent de;
  struct stat st;

  fd = open(path, O_RDONLY);
  if (fd < 0) {
    uprintf("ls: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    uprintf("ls: cannot fstat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_FILE) {
    strcpy(de.name, path);
    ls_file(&st, de.name);
    close(fd);
    return;
  }

  while ((n = read(fd, (char *)&de, sizeof(de))) == sizeof(de)) {
    if (de.inum == 0)
      continue;

    make_path(fullpath, path, de.name);

    fd2 = open(fullpath, O_RDONLY);
    if (fd2 < 0) {
      uprintf("ls: cannot open %s\n", fullpath);
      continue;
    }

    if (fstat(fd2, &st) < 0) {
      uprintf("ls: cannot fstat %s\n", fullpath);
      close(fd2);
      continue;
    }

    ls_file(&st, de.name);
    close(fd2);
  }

  if (n < 0)
    uprintf("ls: read error %s\n", path);

  close(fd);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    ls(".");
  } else {
    for (int i = 1; i < argc; i++)
      ls(argv[i]);
  }

  exit(0);
  return 0;
}
