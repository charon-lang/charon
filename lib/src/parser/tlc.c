#include "ast/node.h"
#include "charon/diag.h"
#include "lexer/tokenizer.h"
#include "lib/alloc.h"
#include "lib/context.h"
#include "lib/diag.h"
#include "parser.h"
#include "parser/util.h"

static ast_node_t *parse_type(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_TYPE));
    token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);

    const char **parameters = NULL;
    size_t parameter_count = 0;
    if(util_try_consume(tokenizer, TOKEN_KIND_CARET_LEFT)) {
        do {
            parameters = alloc_array(parameters, ++parameter_count, sizeof(const char *));
            parameters[parameter_count - 1] = util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER));
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
        util_consume(tokenizer, TOKEN_KIND_CARET_RIGHT);
    }
    ast_type_t *type = util_parse_type(tokenizer);

    return ast_node_make_tlc_type_definition(util_text_make_from_token(tokenizer, token_identifier), type, parameter_count, parameters, attributes, source_location);
}

static ast_node_t *parse_module(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_MODULE));
    token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    const char *module_name = util_text_make_from_token(tokenizer, token_identifier);

    ast_node_list_t tlcs = AST_NODE_LIST_INIT;
    util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) {
        if(tokenizer_is_eof(tokenizer)) {
            diag_t *diag_ref = diag(util_loc(tokenizer, tokenizer_peek(tokenizer)), (diag_t) { .type = DIAG_TYPE__UNFINISHED_MODULE, .data.unfinished_module_name = module_name });
            return ast_node_make_error(diag_ref);
        }

        ast_node_list_append(&tlcs, parser_tlc(tokenizer));
    }

    return ast_node_make_tlc_module(module_name, tlcs, attributes, source_location);
}

static ast_node_t *parse_function(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION));

    const char *name = util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER));

    const char **parameters = NULL;
    size_t parameter_count = 0;
    if(util_try_consume(tokenizer, TOKEN_KIND_CARET_LEFT)) {
        do {
            parameters = alloc_array(parameters, ++parameter_count, sizeof(const char *));
            parameters[parameter_count - 1] = util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER));
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
        util_consume(tokenizer, TOKEN_KIND_CARET_RIGHT);
    }

    const char **argument_names = NULL;
    ast_type_function_t *function_type = util_parse_function_type(tokenizer, &argument_names);

    return ast_node_make_tlc_function(name, function_type, argument_names, parser_top_stmt(tokenizer), parameter_count, parameters, attributes, source_location);
}

static ast_node_t *parse_extern(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_EXTERN));

    util_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION);
    const char *name = util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER));
    ast_type_function_t *function_type = util_parse_function_type(tokenizer, NULL);

    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    return ast_node_make_tlc_extern(name, function_type, attributes, source_location);
}

static ast_node_t *parse_declaration(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_LET));
    token_t token_name = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    util_consume(tokenizer, TOKEN_KIND_COLON);
    ast_type_t *type = util_parse_type(tokenizer);
    ast_node_t *initial = NULL;
    if(util_try_consume(tokenizer, TOKEN_KIND_EQUAL)) initial = parser_expr(tokenizer);
    return ast_node_make_tlc_declaration(util_text_make_from_token(tokenizer, token_name), type, initial, attributes, source_location);
}

static ast_node_t *parse_enum(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_ENUM));
    token_t token_name = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);

    size_t member_count = 0;
    const char **members = NULL;
    if(tokenizer_peek(tokenizer).kind == TOKEN_KIND_IDENTIFIER) {
        do {
            token_t token_member = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
            members = alloc_array(members, ++member_count, sizeof(const char *));
            members[member_count - 1] = util_text_make_from_token(tokenizer, token_member);
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
    }
    util_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT);
    return ast_node_make_tlc_enumeration(util_text_make_from_token(tokenizer, token_name), member_count, members, attributes, source_location);
}

ast_node_t *parser_tlc(tokenizer_t *tokenizer) {
    context_recovery_boundary_t *boundary = context_recover_boundary_push();
    if(setjmp(boundary->jmpbuf) != 0) goto recover;

    ast_node_t *node = NULL;
    ast_attribute_list_t attributes = util_parse_ast_attributes(tokenizer);
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        case TOKEN_KIND_KEYWORD_MODULE:   node = parse_module(tokenizer, attributes); break;
        case TOKEN_KIND_KEYWORD_FUNCTION: node = parse_function(tokenizer, attributes); break;
        case TOKEN_KIND_KEYWORD_EXTERN:   node = parse_extern(tokenizer, attributes); break;
        case TOKEN_KIND_KEYWORD_TYPE:     node = parse_type(tokenizer, attributes); break;
        case TOKEN_KIND_KEYWORD_LET:      node = parse_declaration(tokenizer, attributes); break;
        case TOKEN_KIND_KEYWORD_ENUM:     node = parse_enum(tokenizer, attributes); break;
        default:                          g_global_context->recovery.associated_diag = diag(util_loc(tokenizer, token), (diag_t) { .type = DIAG_TYPE__EXPECTED_TLC }); goto recover;
    }
    context_recover_boundary_pop();
    return node;

recover:
    while(!tokenizer_is_eof(tokenizer)) {
        switch(tokenizer_peek(tokenizer).kind) {
            case TOKEN_KIND_AT:
            case TOKEN_KIND_KEYWORD_MODULE:
            case TOKEN_KIND_KEYWORD_FUNCTION:
            case TOKEN_KIND_KEYWORD_EXTERN:
            case TOKEN_KIND_KEYWORD_TYPE:
            case TOKEN_KIND_KEYWORD_LET:
            case TOKEN_KIND_KEYWORD_ENUM:     break;
            default:                          tokenizer_advance(tokenizer); continue;
        }
        break;
    }
    context_recover_boundary_pop();
    return ast_node_make_error(g_global_context->recovery.associated_diag);
}
