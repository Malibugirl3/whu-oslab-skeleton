#include "user.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        uprintf("Usage: mkdir dir...\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        if (mkdir(argv[i]) < 0)
            uprintf("mkdir: cannot create directory %s\n", argv[i]);
   }

   exit(0);
   return 0;
}