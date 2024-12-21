#include "parser.h"

#include "lib/diag.h"
#include "lib/alloc.h"
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

    ast_node_list_t tlcs = AST_NODE_LIST_INIT;
    util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) ast_node_list_append(&tlcs, parser_tlc(tokenizer));

    return ast_node_make_tlc_module(util_text_make_from_token(tokenizer, token_identifier), tlcs, attributes, source_location);
}

static ast_node_t *parse_function(tokenizer_t *tokenizer, ast_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION));

    const char *name = util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER));
    const char **argument_names = NULL;
    ast_type_function_t *function_type = util_parse_function_type(tokenizer, &argument_names);

    return ast_node_make_tlc_function(name, function_type, argument_names, parser_stmt(tokenizer), attributes, source_location);
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
    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
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
    ast_attribute_list_t attributes = util_parse_ast_attributes(tokenizer);
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        case TOKEN_KIND_KEYWORD_MODULE: return parse_module(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_FUNCTION: return parse_function(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_EXTERN: return parse_extern(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_TYPE: return parse_type(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_LET: return parse_declaration(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_ENUM: return parse_enum(tokenizer, attributes);
        default: diag_error(util_loc(tokenizer, token), LANG_E_EXPECTED_TLC, token_kind_stringify(token.kind));
    }
    __builtin_unreachable();
}
