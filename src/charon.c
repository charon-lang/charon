#include "lib/source.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    printf("Charon (dev)\n");

    source_t *source = source_from_path("tests/00.charon");

    source_free(source);

    return EXIT_SUCCESS;
}