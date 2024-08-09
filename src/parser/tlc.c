#include "parser.h"

#include "lib/diag.h"
#include "ir/function.h"
#include "parser/util.h"

#include <stdlib.h>

static ir_function_t *parse_prototype(tokenizer_t *tokenizer) {
    util_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION);

    ir_function_t *prototype = ir_function_make(util_text_make_from_token(tokenizer, util_consume(tokenizer, TOKEN_KIND_IDENTIFIER)));
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);
    if(tokenizer_peek(tokenizer).kind != TOKEN_KIND_PARENTHESES_RIGHT) {
        do {
            if(util_try_consume(tokenizer, TOKEN_KIND_TRIPLE_PERIOD)) {
                prototype->varargs = true;
                break;
            }

            token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
            util_consume(tokenizer, TOKEN_KIND_COLON);
            ir_type_t *type = util_parse_type(tokenizer);
            prototype->arguments = realloc(prototype->arguments, ++prototype->argument_count * sizeof(ir_function_argument_t));
            prototype->arguments[prototype->argument_count - 1] = (ir_function_argument_t) { .type = type, .name = util_text_make_from_token(tokenizer, token_identifier) };
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
    }
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);

    if(util_try_consume(tokenizer, TOKEN_KIND_COLON)) {
        prototype->return_type = util_parse_type(tokenizer);
    } else {
        prototype->return_type = ir_type_get_void();
    }

    return prototype;
}

static ir_node_t *parse_function(tokenizer_t *tokenizer) {
    source_location_t source_location = util_loc(tokenizer, tokenizer_peek(tokenizer));
    ir_function_t *prototype = parse_prototype(tokenizer);

    ir_node_list_t statements = IR_NODE_LIST_INIT;
    util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);
    while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) ir_node_list_append(&statements, parser_stmt(tokenizer));
    return ir_node_make_tlc_function(prototype, statements, source_location);
}

ir_node_t *parse_extern(tokenizer_t *tokenizer) {
    source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_EXTERN));
    ir_function_t *prototype = parse_prototype(tokenizer);
    util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);
    return ir_node_make_tlc_extern(prototype, source_location);
}

ir_node_t *parser_tlc(tokenizer_t *tokenizer) {
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        case TOKEN_KIND_KEYWORD_FUNCTION: return parse_function(tokenizer);
        case TOKEN_KIND_KEYWORD_EXTERN: return parse_extern(tokenizer);
        default: diag_error(util_loc(tokenizer, token), "expected top level construct, got %s", token_kind_tostring(token.kind));
    }
    __builtin_unreachable();
}
