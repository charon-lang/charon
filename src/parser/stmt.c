#include "parser.h"

#include "parser/util.h"

static ir_node_t *parse_expression(tokenizer_t *tokenizer) {
    ir_node_t *expression = parser_expr(tokenizer);
    return ir_node_make_stmt_expression(expression, expression->source_location);
}

static ir_node_t *parse_simple(tokenizer_t *tokenizer) {
    ir_node_t *node = parse_expression(tokenizer);
    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    return node;
}

static ir_node_t *parse_block(tokenizer_t *tokenizer) {
    source_location_t source_location = UTIL_SRCLOC(tokenizer, util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT));
    ir_node_list_t statements = IR_NODE_LIST_INIT;
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) ir_node_list_append(&statements, parser_stmt(tokenizer));
    return ir_node_make_stmt_block(statements, source_location);
}

ir_node_t *parser_stmt(tokenizer_t *tokenizer) {
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        case TOKEN_KIND_BRACE_LEFT: return parse_block(tokenizer);
        default: return parse_simple(tokenizer);
    }
    __builtin_unreachable();
}