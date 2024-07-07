#include "parser.h"

#include "lib/diag.h"
#include "parser/util.h"

static ir_node_t *parse_function(tokenizer_t *tokenizer) {
    source_location_t source_location = UTIL_SRCLOC(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION));
    token_t token_name = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);

    ir_node_list_t statements = IR_NODE_LIST_INIT;
    util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) ir_node_list_append(&statements, parser_stmt(tokenizer));
    return ir_node_make_tlc_function(util_text_make_from_token(tokenizer, token_name), statements, source_location);
}

ir_node_t *parser_tlc(tokenizer_t *tokenizer) {
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        case TOKEN_KIND_KEYWORD_FUNCTION: return parse_function(tokenizer);
        default: diag_error(UTIL_SRCLOC(tokenizer, token), "expected top level construct, got %s", token_kind_tostring(token.kind));
    }
    __builtin_unreachable();
}