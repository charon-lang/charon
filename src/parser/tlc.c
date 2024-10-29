#include "parser.h"

#include "lib/diag.h"
#include "parser/util.h"

#include <stdlib.h>

static ir_type_t *parse_prototype(tokenizer_t *tokenizer, const char ***argument_names) {
    ir_type_t *prototype = ir_type_function_make();
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);
    if(tokenizer_peek(tokenizer).kind != TOKEN_KIND_PARENTHESES_RIGHT) {
        do {
            if(util_try_consume(tokenizer, TOKEN_KIND_TRIPLE_PERIOD)) {
                ir_type_function_set_varargs(prototype);
                break;
            }

            token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
            util_consume(tokenizer, TOKEN_KIND_COLON);

            ir_type_t *type = util_parse_type(tokenizer);
            ir_type_function_add_argument(prototype, type);

            *argument_names = reallocarray(*argument_names, prototype->function.argument_count, sizeof(char *));
            (*argument_names)[prototype->function.argument_count - 1] = util_text_make_from_token(tokenizer, token_identifier);
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
    }
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);

    if(util_try_consume(tokenizer, TOKEN_KIND_COLON)) ir_type_function_set_return_type(prototype, util_parse_type(tokenizer));

    return prototype;
}

static ir_node_t *parse_module(tokenizer_t *tokenizer) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_MODULE));
    token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);

    ir_node_list_t tlcs = IR_NODE_LIST_INIT;
    util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) ir_node_list_append(&tlcs, parser_tlc(tokenizer));

    return ir_node_make_tlc_module(util_text_make_from_token(tokenizer, token_identifier), tlcs, source_location);
}

static ir_node_t *parse_function(tokenizer_t *tokenizer) {
    source_location_t source_location = util_loc(tokenizer, tokenizer_peek(tokenizer));

    util_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION);
    const char *name = util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER));
    const char **argument_names = NULL;
    ir_type_t *prototype = parse_prototype(tokenizer, &argument_names);

    return ir_node_make_tlc_function(name, prototype, argument_names, parser_stmt(tokenizer), source_location);
}

ir_node_t *parse_extern(tokenizer_t *tokenizer) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_EXTERN));

    util_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION);
    const char *name = util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER));
    const char **argument_names = NULL;
    ir_type_t *prototype = parse_prototype(tokenizer, &argument_names);

    for(size_t i = 0; i < prototype->function.argument_count; i++) free((char *) argument_names[i]);
    free(argument_names);

    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    return ir_node_make_tlc_extern(name, prototype, source_location);
}

ir_node_t *parser_tlc(tokenizer_t *tokenizer) {
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        case TOKEN_KIND_KEYWORD_MODULE: return parse_module(tokenizer);
        case TOKEN_KIND_KEYWORD_FUNCTION: return parse_function(tokenizer);
        case TOKEN_KIND_KEYWORD_EXTERN: return parse_extern(tokenizer);
        default: diag_error(util_loc(tokenizer, token), "expected top level construct, got %s", token_kind_tostring(token.kind));
    }
    __builtin_unreachable();
}
