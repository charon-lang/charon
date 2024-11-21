#include "parser.h"

#include "lib/diag.h"
#include "parser/util.h"

#include <stdlib.h>

static hlir_node_t *parse_type(tokenizer_t *tokenizer, hlir_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_TYPE));
    token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);

    return hlir_node_make_tlc_type_definition(util_text_make_from_token(tokenizer, token_identifier), util_parse_type(tokenizer), attributes, source_location);
}

static hlir_node_t *parse_module(tokenizer_t *tokenizer, hlir_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_MODULE));
    token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);

    hlir_node_list_t tlcs = HLIR_NODE_LIST_INIT;
    util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) hlir_node_list_append(&tlcs, parser_tlc(tokenizer));

    return hlir_node_make_tlc_module(util_text_make_from_token(tokenizer, token_identifier), tlcs, attributes, source_location);
}

static hlir_node_t *parse_function(tokenizer_t *tokenizer, hlir_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, tokenizer_peek(tokenizer));

    util_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION);
    const char *name = util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER));
    const char **argument_names = NULL;
    hlir_type_function_t *function_type = util_parse_function_type(tokenizer, &argument_names);

    return hlir_node_make_tlc_function(name, function_type, argument_names, parser_stmt(tokenizer), attributes, source_location);
}

static hlir_node_t *parse_extern(tokenizer_t *tokenizer, hlir_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_EXTERN));

    util_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION);
    const char *name = util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER));
    hlir_type_function_t *function_type = util_parse_function_type(tokenizer, NULL);

    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    return hlir_node_make_tlc_extern(name, function_type, attributes, source_location);
}

static hlir_node_t *parse_declaration(tokenizer_t *tokenizer, hlir_attribute_list_t attributes) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_LET));
    token_t token_name = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    util_consume(tokenizer, TOKEN_KIND_COLON);
    hlir_type_t *type = util_parse_type(tokenizer);
    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    return hlir_node_make_tlc_declaration(util_text_make_from_token(tokenizer, token_name), type, attributes, source_location);
}

hlir_node_t *parser_tlc(tokenizer_t *tokenizer) {
    hlir_attribute_list_t attributes = util_parse_hlir_attributes(tokenizer);
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        case TOKEN_KIND_KEYWORD_MODULE: return parse_module(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_FUNCTION: return parse_function(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_EXTERN: return parse_extern(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_TYPE: return parse_type(tokenizer, attributes);
        case TOKEN_KIND_KEYWORD_LET: return parse_declaration(tokenizer, attributes);
        default: diag_error(util_loc(tokenizer, token), "expected top level construct, got %s", token_kind_stringify(token.kind));
    }
    __builtin_unreachable();
}
