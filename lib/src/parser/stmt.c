#include "ast/attribute.h"
#include "ast/node.h"
#include "lexer/token.h"
#include "lexer/tokenizer.h"
#include "lib/alloc.h"
#include "lib/context.h"
#include "lib/diag.h"
#include "parser.h"
#include "parser/util.h"

#include <stddef.h>

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

static ast_node_t *parse_switch(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    token_t token_switch = util_consume(tokenizer, TOKEN_KIND_KEYWORD_SWITCH);

    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);
    ast_node_t *value = parser_expr(tokenizer);
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);

    size_t case_count = 0;
    ast_node_switch_case_t *cases = NULL;
    ast_node_t *default_body = NULL;
    util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) {
        if(tokenizer_peek(tokenizer).kind == TOKEN_KIND_KEYWORD_DEFAULT) {
            token_t default_token = util_consume(tokenizer, TOKEN_KIND_KEYWORD_DEFAULT);
            if(default_body != NULL) {
                diag_t *diag_ref = diag(util_loc(tokenizer, default_token), (diag_t) { .type = DIAG_TYPE__DUPLICATE_DEFAULT });
                return ast_node_make_error(diag_ref);
            }

            token_t token_arrow = util_consume(tokenizer, TOKEN_KIND_THICK_ARROW);
            ast_node_t *case_body = parse_stmt(tokenizer);
            if(case_body == NULL) {
                diag_t *diag_ref = diag(util_loc(tokenizer, token_arrow), (diag_t) { .type = DIAG_TYPE__EXPECTED_STATEMENT });
                return ast_node_make_error(diag_ref);
            }

            default_body = case_body;
            continue;
        }

        ast_node_t *case_value = parser_expr(tokenizer);
        token_t token_arrow = util_consume(tokenizer, TOKEN_KIND_THICK_ARROW);
        ast_node_t *case_body = parse_stmt(tokenizer);
        if(case_body == NULL) {
            diag_t *diag_ref = diag(util_loc(tokenizer, token_arrow), (diag_t) { .type = DIAG_TYPE__EXPECTED_STATEMENT });
            return ast_node_make_error(diag_ref);
        }

        cases = alloc_array(cases, ++case_count, sizeof(ast_node_switch_case_t));
        cases[case_count - 1].value = case_value;
        cases[case_count - 1].body = case_body;
    };

    return ast_node_make_stmt_switch(value, case_count, cases, default_body, attributes, util_loc(tokenizer, token_switch));
}

static ast_node_t *parse_block(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT));
    ast_node_list_t statements = AST_NODE_LIST_INIT;
    while(!tokenizer_is_eof(tokenizer) && !util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) {
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
        case TOKEN_KIND_SEMI_COLON:     util_consume(tokenizer, TOKEN_KIND_SEMI_COLON); return NULL;
        case TOKEN_KIND_KEYWORD_IF:     return parse_if(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_WHILE:  return parse_while(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_FOR:    return parse_for(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_SWITCH: return parse_switch(tokenizer, attributes);
        case TOKEN_KIND_BRACE_LEFT:     return parse_block(tokenizer, attributes);
        default:                        return parse_simple(tokenizer, attributes);
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
