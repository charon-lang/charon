#include "parser.h"

#include "parser/util.h"

static ir_node_t *parse_expression(tokenizer_t *tokenizer) {
    ir_node_t *expression = parser_expr(tokenizer);
    return ir_node_make_stmt_expression(expression, expression->source_location);
}

static ir_node_t *parse_declaration(tokenizer_t *tokenizer) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_LET));
    token_t token_name = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    ir_type_t *type = NULL;
    if(util_try_consume(tokenizer, TOKEN_KIND_COLON)) type = util_parse_type(tokenizer);
    ir_node_t *initial = NULL;
    if(util_try_consume(tokenizer, TOKEN_KIND_EQUAL)) initial = parser_expr(tokenizer);
    return ir_node_make_stmt_declaration(util_text_make_from_token(tokenizer, token_name), type, initial, source_location);
}

static ir_node_t *parse_simple(tokenizer_t *tokenizer) {
    ir_node_t *node;
    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_KEYWORD_LET: node = parse_declaration(tokenizer); break;
        default: node = parse_expression(tokenizer); break;
    }
    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    return node;
}

static ir_node_t *parse_block(tokenizer_t *tokenizer) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT));
    ir_node_list_t statements = IR_NODE_LIST_INIT;
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) ir_node_list_append(&statements, parser_stmt(tokenizer));
    return ir_node_make_stmt_block(statements, source_location);
}

ir_node_t *parser_stmt(tokenizer_t *tokenizer) {
    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_BRACE_LEFT: return parse_block(tokenizer);
        default: return parse_simple(tokenizer);
    }
    __builtin_unreachable();
}
