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

ir_node_t *parser_stmt(tokenizer_t *tokenizer) {
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        default: return parse_simple(tokenizer);
    }
    __builtin_unreachable();
}