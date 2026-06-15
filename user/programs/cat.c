#include "user.h"

static void cat_file(const char *path) {
    char buf[128];
    int fd;
    int n = -1;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        uprintf("cat: cannot open %s\n", path);
        return;
    }

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
    }

    if (n < 0) {
        uprintf("cat: cannot read %s\n", path);
    }
    else if (n == 0) {
        uprintf("cat: end of file %s\n", path);
    }

    close(fd);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        uprintf("Usage: cat file...\n");
        exit(1);
    }
    for (int i = 1; i < argc; i++) {
        cat_file(argv[i]);
    }

    exit(0);
    return 0;
}