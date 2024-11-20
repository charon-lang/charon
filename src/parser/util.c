#include "util.h"

#include "primitive.h"
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
    hlir_attribute_list_t attributes = util_parse_hlir_attributes(tokenizer);

    if(util_try_consume(tokenizer, TOKEN_KIND_KEYWORD_STRUCT)) {
        util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);

        size_t member_count = 0;
        hlir_type_structure_member_t *members = NULL;
        while(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) {
            token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
            util_consume(tokenizer, TOKEN_KIND_COLON);
            hlir_type_t *type = util_parse_type(tokenizer);
            util_consume(tokenizer, TOKEN_KIND_SEMI_COLON);

            members = reallocarray(members, ++member_count, sizeof(hlir_type_structure_member_t));
            members[member_count - 1] = (hlir_type_structure_member_t) { .type = type, .name = util_text_make_from_token(tokenizer, token_identifier) };
        }
        return hlir_type_structure_make(member_count, members, attributes);
    }

    if(util_try_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT)) {
        size_t count = 0;
        hlir_type_t **types = NULL;
        do {
            types = reallocarray(types, ++count, sizeof(hlir_type_t *));
            types[count - 1] = util_parse_type(tokenizer);
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
        util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
        return hlir_type_tuple_make(count, types, attributes);
    }
    if(util_try_consume(tokenizer, TOKEN_KIND_BRACKET_LEFT)) {
        uintmax_t size = util_number_make_from_token(tokenizer, tokenizer_advance(tokenizer));
        util_consume(tokenizer, TOKEN_KIND_BRACKET_RIGHT);
        return hlir_type_array_make(util_parse_type(tokenizer), size, attributes);
    }
    if(util_try_consume(tokenizer, TOKEN_KIND_STAR)) return hlir_type_pointer_make(util_parse_type(tokenizer), attributes);

    token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    if(util_token_cmp(tokenizer, token_identifier, "bool") == 0) return hlir_type_integer_make(1, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "char") == 0) return hlir_type_integer_make(PRIMITIVE_CHAR_SIZE, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "ptr") == 0) return hlir_type_integer_make(PRIMITIVE_POINTER_SIZE, false, attributes);

    if(util_token_cmp(tokenizer, token_identifier, "uint") == 0) return hlir_type_integer_make(PRIMITIVE_INT_SIZE, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "u8") == 0) return hlir_type_integer_make(8, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "u16") == 0) return hlir_type_integer_make(16, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "u32") == 0) return hlir_type_integer_make(32, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "u64") == 0) return hlir_type_integer_make(64, false, attributes);

    if(util_token_cmp(tokenizer, token_identifier, "int") == 0) return hlir_type_integer_make(PRIMITIVE_INT_SIZE, true, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "i8") == 0) return hlir_type_integer_make(8, true, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "i16") == 0) return hlir_type_integer_make(16, true, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "i32") == 0) return hlir_type_integer_make(32, true, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "i64") == 0) return hlir_type_integer_make(64, true, attributes);

    return hlir_type_reference_make(util_text_make_from_token(tokenizer, token_identifier), attributes);
}

hlir_attribute_list_t util_parse_hlir_attributes(tokenizer_t *tokenizer) {
    hlir_attribute_list_t list = HLIR_ATTRIBUTE_LIST_INIT;

    while(util_try_consume(tokenizer, TOKEN_KIND_AT)) {
        token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
        if(util_token_cmp(tokenizer, token_identifier, "packed") == 0) {
            hlir_attribute_list_add_packed(&list, util_loc(tokenizer, token_identifier));
        }
        else diag_error(util_loc(tokenizer, token_identifier), "invalid attribute");
    }

    return list;
}