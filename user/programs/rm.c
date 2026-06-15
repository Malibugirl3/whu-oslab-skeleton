#include "user.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        uprintf("Usage: rm file...\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        if (unlink(argv[i]) < 0) {
            uprintf("rm: cannot remove %s\n", argv[i]);
        }
    }

    exit(0);
    return 0;
}