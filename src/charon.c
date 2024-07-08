#include "lib/source.h"
#include "lexer/tokenizer.h"
#include "parser/parser.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    printf("Charon (dev)\n");

    source_t *source = source_from_path("tests/00.charon");
    tokenizer_t *tokenizer = tokenizer_make(source);

    ir_node_t *root = parser_root(tokenizer);

    tokenizer_free(tokenizer);
    source_free(source);

    return EXIT_SUCCESS;
}