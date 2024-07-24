#include "parser.h"

#include "parser/util.h"
#include "lexer/token.h"
#include "lexer/tokenizer.h"

#include <stdlib.h>

ir_node_t *parser_root(tokenizer_t *tokenizer) {
    source_location_t source_location = util_loc(tokenizer, tokenizer_peek(tokenizer));
    ir_node_list_t tlc_nodes = IR_NODE_LIST_INIT;
    while(!tokenizer_is_eof(tokenizer)) ir_node_list_append(&tlc_nodes, parser_tlc(tokenizer));
    return ir_node_make_root(tlc_nodes, source_location);
}