#include "util.h"

#include "lib/source.h"
#include "lib/diag.h"

#include <string.h>
#include <stdlib.h>

source_location_t util_loc(tokenizer_t *tokenizer, token_t token) {
    return (source_location_t) { .source = tokenizer->source, .offset = token.offset, .length = token.size };
}

bool util_try_consume(tokenizer_t *tokenizer, token_kind_t kind) {
    if(tokenizer_peek(tokenizer).kind == kind) {
        tokenizer_advance(tokenizer);
        return true;
    }
    return false;
}

token_t util_consume(tokenizer_t *tokenizer, token_kind_t kind) {
    token_t token = tokenizer_advance(tokenizer);
    if(token.kind == kind) return token;
    diag_error(util_loc(tokenizer, token), "expected %s got %s", token_kind_tostring(kind), token_kind_tostring(token.kind));
}

char *util_text_make_from_token(tokenizer_t *tokenizer, token_t token) {
    char *text = malloc(token.size + 1);
    memcpy(text, tokenizer->source->data_buffer + token.offset, token.size);
    text[token.size] = '\0';
    return text;
}

int util_token_cmp(tokenizer_t *tokenizer, token_t token, const char *string) {
    return strncmp(tokenizer->source->data_buffer + token.offset, string, token.size);
}

ir_type_t *util_parse_type(tokenizer_t *tokenizer) {
    if(util_try_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT)) {
        size_t count = 0;
        ir_type_t **types = NULL;
        do {
            types = realloc(types, ++count * sizeof(ir_type_t *));
            types[count - 1] = util_parse_type(tokenizer);
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
        util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
        return ir_type_tuple_make(count, types);
    }
    if(util_try_consume(tokenizer, TOKEN_KIND_STAR)) return ir_type_pointer_make(util_parse_type(tokenizer));
    token_t token_primitive_type = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    if(util_token_cmp(tokenizer, token_primitive_type, "void") == 0) return ir_type_get_void();
    if(util_token_cmp(tokenizer, token_primitive_type, "bool") == 0) return ir_type_get_bool();
    if(util_token_cmp(tokenizer, token_primitive_type, "char") == 0) return ir_type_get_char();
    if(util_token_cmp(tokenizer, token_primitive_type, "uint") == 0) return ir_type_get_uint();
    if(util_token_cmp(tokenizer, token_primitive_type, "u8") == 0) return ir_type_get_u8();
    if(util_token_cmp(tokenizer, token_primitive_type, "u16") == 0) return ir_type_get_u16();
    if(util_token_cmp(tokenizer, token_primitive_type, "u32") == 0) return ir_type_get_u32();
    if(util_token_cmp(tokenizer, token_primitive_type, "u64") == 0) return ir_type_get_u64();
    if(util_token_cmp(tokenizer, token_primitive_type, "int") == 0) return ir_type_get_int();
    if(util_token_cmp(tokenizer, token_primitive_type, "i8") == 0) return ir_type_get_i8();
    if(util_token_cmp(tokenizer, token_primitive_type, "i16") == 0) return ir_type_get_i16();
    if(util_token_cmp(tokenizer, token_primitive_type, "i32") == 0) return ir_type_get_i32();
    if(util_token_cmp(tokenizer, token_primitive_type, "i64") == 0) return ir_type_get_i64();
    diag_error(util_loc(tokenizer, token_primitive_type), "invalid type `%s`", util_text_make_from_token(tokenizer, token_primitive_type));
}
