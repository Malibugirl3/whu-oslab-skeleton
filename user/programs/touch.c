#include "user.h"

int main(int argc, char **argv) {
    int fd;

    if (argc < 2) {
        uprintf("Usage: touch file...\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        fd = open(argv[i], O_CREAT | O_WRONLY);
        
        if (fd < 0) {
            uprintf("touch: cannot create %s\n", argv[i]);
            continue;
        }
        close(fd);
    }

    exit(0);
    return 0;
}