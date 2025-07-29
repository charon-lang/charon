#include "lexer/tokenizer.h"
#include "parser.h"
#include "parser/util.h"

ast_node_t *parser_root(tokenizer_t *tokenizer) {
    ast_node_list_t tlcs = AST_NODE_LIST_INIT;
    while(!tokenizer_is_eof(tokenizer)) ast_node_list_append(&tlcs, parser_tlc(tokenizer));
    return ast_node_make_root(tlcs, AST_ATTRIBUTE_LIST_INIT, util_loc(tokenizer, tokenizer_peek(tokenizer)));
}
