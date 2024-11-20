#include "parser.h"

#include "parser/util.h"
#include "lexer/token.h"
#include "lexer/tokenizer.h"

#include <stdlib.h>

hlir_node_t *parser_root(tokenizer_t *tokenizer) {
    hlir_node_list_t tlcs = HLIR_NODE_LIST_INIT;
    while(!tokenizer_is_eof(tokenizer)) hlir_node_list_append(&tlcs, parser_tlc(tokenizer));
    return hlir_node_make_root(tlcs, HLIR_ATTRIBUTE_LIST_INIT, util_loc(tokenizer, tokenizer_peek(tokenizer)));
}