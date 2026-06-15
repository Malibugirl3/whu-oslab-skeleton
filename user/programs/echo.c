#include "user.h"

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        write(1, argv[i], strlen(argv[i]));
        if (i < argc - 1) 
            write(1, " ", 1);
    }

    write(1, "\n", 1);
    exit(0);
    return 0;
}