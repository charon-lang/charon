#include "ast/node.h"
#include "lib/context.h"
#include "parser.h"
#include "parser/util.h"

static ast_node_t *parse_stmt(tokenizer_t *tokenizer);

static ast_node_t *parse_expression(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    ast_node_t *expression = parser_expr(tokenizer);
    return ast_node_make_stmt_expression(expression, attributes, expression->source_location);
}

static ast_node_t *parse_declaration(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_LET));
    token_t token_name = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    ast_type_t *type = NULL;
    if(util_try_consume(tokenizer, TOKEN_KIND_COLON)) type = util_parse_type(tokenizer);
    ast_node_t *initial = NULL;
    if(util_try_consume(tokenizer, TOKEN_KIND_EQUAL)) initial = parser_expr(tokenizer);
    return ast_node_make_stmt_declaration(util_text_make_from_token(tokenizer, token_name), type, initial, attributes, source_location);
}

static ast_node_t *parse_return(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    token_t token_return = util_consume(tokenizer, TOKEN_KIND_KEYWORD_RETURN);
    ast_node_t *value = NULL;
    if(tokenizer_peek(tokenizer).kind != TOKEN_KIND_SEMI_COLON) value = parser_expr(tokenizer);
    return ast_node_make_stmt_return(value, attributes, util_loc(tokenizer, token_return));
}

static ast_node_t *parse_if(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    token_t token_if = util_consume(tokenizer, TOKEN_KIND_KEYWORD_IF);
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);
    ast_node_t *condition = parser_expr(tokenizer);
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
    ast_node_t *body = parse_stmt(tokenizer);
    ast_node_t *else_body = NULL;
    if(util_try_consume(tokenizer, TOKEN_KIND_KEYWORD_ELSE)) else_body = parse_stmt(tokenizer);
    return ast_node_make_stmt_if(condition, body, else_body, attributes, util_loc(tokenizer, token_if));
}

static ast_node_t *parse_while(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    token_t token_while = util_consume(tokenizer, TOKEN_KIND_KEYWORD_WHILE);
    ast_node_t *condition = NULL;
    if(util_try_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT)) {
        condition = parser_expr(tokenizer);
        util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
    }
    ast_node_t *body = parse_stmt(tokenizer);
    return ast_node_make_stmt_while(condition, body, attributes, util_loc(tokenizer, token_while));
}

static ast_node_t *parse_for(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    token_t token_for = util_consume(tokenizer, TOKEN_KIND_KEYWORD_FOR);
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);

    ast_node_t *decl = NULL;
    if(!util_try_consume(tokenizer, TOKEN_KIND_SEMI_COLON)) {
        decl = parse_declaration(tokenizer, util_parse_ast_attributes(tokenizer));
        util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    }

    ast_node_t *condition = NULL;
    if(!util_try_consume(tokenizer, TOKEN_KIND_SEMI_COLON)) {
        condition = parser_expr(tokenizer);
        util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    }

    ast_node_t *after = NULL;
    if(!util_try_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT)) {
        after = parser_expr(tokenizer);
        util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
    }

    ast_node_t *body = parse_stmt(tokenizer);

    return ast_node_make_stmt_for(decl, condition, after, body, attributes, util_loc(tokenizer, token_for));
}

static ast_node_t *parse_block(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT));
    ast_node_list_t statements = AST_NODE_LIST_INIT;
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) {
        context_recovery_boundary_t *boundary = context_recover_boundary_push();
        if(setjmp(boundary->jmpbuf) != 0) {
            context_recover_boundary_pop();
            ast_node_list_append(&statements, ast_node_make_error(g_global_context->recovery.associated_diag));
            continue;
        }

        ast_node_t *statement = parse_stmt(tokenizer);
        if(statement != NULL) ast_node_list_append(&statements, statement);
        context_recover_boundary_pop();
    }
    return ast_node_make_stmt_block(statements, attributes, source_location);
}

static ast_node_t *parse_simple(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    ast_node_t *node;
    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_KEYWORD_RETURN:   node = parse_return(tokenizer, attributes); break;
        case TOKEN_KIND_KEYWORD_LET:      node = parse_declaration(tokenizer, attributes); break;
        case TOKEN_KIND_KEYWORD_CONTINUE: node = ast_node_make_stmt_continue(attributes, util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_CONTINUE))); break;
        case TOKEN_KIND_KEYWORD_BREAK:    node = ast_node_make_stmt_break(attributes, util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_BREAK))); break;
        default:                          node = parse_expression(tokenizer, attributes); break;
    }
    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    return node;
}

ast_node_t *parse_stmt(tokenizer_t *tokenizer) {
    ast_attribute_list_t attributes = util_parse_ast_attributes(tokenizer);
    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_SEMI_COLON:    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON); return NULL;
        case TOKEN_KIND_KEYWORD_IF:    return parse_if(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_WHILE: return parse_while(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_FOR:   return parse_for(tokenizer, attributes);
        case TOKEN_KIND_BRACE_LEFT:    return parse_block(tokenizer, attributes);
        default:                       return parse_simple(tokenizer, attributes);
    }
    __builtin_unreachable();
}

ast_node_t *parser_top_stmt(tokenizer_t *tokenizer) {
    context_recovery_boundary_t *boundary = context_recover_boundary_push();
    if(setjmp(boundary->jmpbuf) != 0) {
        tokenizer_advance(tokenizer);
        context_recover_boundary_pop();
        return ast_node_make_error(g_global_context->recovery.associated_diag);
    }

    ast_node_t *node = parse_stmt(tokenizer);
    context_recover_boundary_pop();
    return node;
}
