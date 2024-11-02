#include "util.h"

#include "lib/source.h"
#include "lib/diag.h"

#include <string.h>
#include <errno.h>
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
    diag_error(util_loc(tokenizer, token), "expected %s got %s", token_kind_stringify(kind), token_kind_stringify(token.kind));
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

uintmax_t util_number_make_from_token(tokenizer_t *tokenizer, token_t token) {
    int base;
    switch(token.kind) {
        case TOKEN_KIND_CONST_NUMBER_DEC: base = 10; break;
        case TOKEN_KIND_CONST_NUMBER_HEX: base = 16; break;
        case TOKEN_KIND_CONST_NUMBER_BIN: base = 2; break;
        case TOKEN_KIND_CONST_NUMBER_OCT: base = 8; break;
        default: diag_error(util_loc(tokenizer, token), "expected a numeric literal");
    }
    char *text = util_text_make_from_token(tokenizer, token);
    errno = 0;
    char *stripped = text;
    if(base != 10) stripped += 2;
    uintmax_t value = strtoull(stripped, NULL, base);
    if(errno == ERANGE) diag_error(util_loc(tokenizer, token), "numeric constant too large");
    free(text);
    return value;
}

hlir_type_t *util_parse_type(tokenizer_t *tokenizer) {
    if(util_try_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT)) {
        size_t count = 0;
        hlir_type_t **types = NULL;
        do {
            types = reallocarray(types, ++count, sizeof(hlir_type_t *));
            types[count - 1] = util_parse_type(tokenizer);
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
        util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
        return hlir_type_tuple_make(count, types);
    }
    if(util_try_consume(tokenizer, TOKEN_KIND_BRACKET_LEFT)) {
        uintmax_t size = util_number_make_from_token(tokenizer, tokenizer_advance(tokenizer));
        util_consume(tokenizer, TOKEN_KIND_BRACKET_RIGHT);
        return hlir_type_array_make(util_parse_type(tokenizer), size);
    }
    if(util_try_consume(tokenizer, TOKEN_KIND_STAR)) return hlir_type_pointer_make(util_parse_type(tokenizer));

    token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    if(util_token_cmp(tokenizer, token_identifier, "bool") == 0) return hlir_type_get_bool();
    if(util_token_cmp(tokenizer, token_identifier, "char") == 0) return hlir_type_get_char();
    if(util_token_cmp(tokenizer, token_identifier, "ptr") == 0) return hlir_type_get_ptr();
    if(util_token_cmp(tokenizer, token_identifier, "uint") == 0) return hlir_type_get_uint();
    if(util_token_cmp(tokenizer, token_identifier, "u8") == 0) return hlir_type_get_u8();
    if(util_token_cmp(tokenizer, token_identifier, "u16") == 0) return hlir_type_get_u16();
    if(util_token_cmp(tokenizer, token_identifier, "u32") == 0) return hlir_type_get_u32();
    if(util_token_cmp(tokenizer, token_identifier, "u64") == 0) return hlir_type_get_u64();
    if(util_token_cmp(tokenizer, token_identifier, "int") == 0) return hlir_type_get_int();
    if(util_token_cmp(tokenizer, token_identifier, "i8") == 0) return hlir_type_get_i8();
    if(util_token_cmp(tokenizer, token_identifier, "i16") == 0) return hlir_type_get_i16();
    if(util_token_cmp(tokenizer, token_identifier, "i32") == 0) return hlir_type_get_i32();
    if(util_token_cmp(tokenizer, token_identifier, "i64") == 0) return hlir_type_get_i64();

    return hlir_type_reference_make(util_text_make_from_token(tokenizer, token_identifier));
}