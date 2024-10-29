#include "lib/source.h"
#include "lexer/tokenizer.h"
#include "parser/parser.h"
#include "codegen/codegen.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if(argc < 3) {
        printf("missing arguments\n");
        return EXIT_FAILURE;
    }

    source_t *source = source_from_path(argv[1]);

    tokenizer_t *tokenizer = tokenizer_make(source);
    ir_node_t *root_node = parser_root(tokenizer);
    tokenizer_free(tokenizer);

    codegen_ir(root_node, argv[2]);

    source_free(source);
    return EXIT_SUCCESS;
}