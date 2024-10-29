#include "parser.h"

#include "parser/util.h"
#include "lexer/token.h"
#include "lexer/tokenizer.h"

#include <stdlib.h>

ir_node_t *parser_root(tokenizer_t *tokenizer) {
    ir_node_t *node = ir_node_make_root(util_loc(tokenizer, tokenizer_peek(tokenizer)));
    while(!tokenizer_is_eof(tokenizer)) ir_node_list_append(&node->root.tlcs, parser_tlc(tokenizer));
    return node;
}