#include <stdio.h>
#include <stdlib.h>

extern void charon_test(const char *path);

int main(int argc, const char **argv) {
    if(argc < 2) {
        printf("no path provided\n");
        exit(EXIT_FAILURE);
    }

    charon_test(argv[1]);

    return 0;
}
