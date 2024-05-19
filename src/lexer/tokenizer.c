#include "tokenizer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pcre2.h>
#include "../diag.h"

typedef struct {
    const char *pattern;
    token_type_t type;
} spec_t;

typedef struct {
    token_type_t type;
    pcre2_code *pattern;
} compiled_spec_t;

static spec_t g_spec[] = {
    { .pattern = "^\\s+" }, // Whitespace
    { .pattern = "^//.*" }, // Single line comment
    { .pattern = "^/\\*[\\s\\S]*?\\*/" }, // Multi line comment

    { .pattern = "^return", .type = TOKEN_TYPE_KEYWORD_RETURN },
    { .pattern = "^if", .type = TOKEN_TYPE_KEYWORD_IF },
    { .pattern = "^else", .type = TOKEN_TYPE_KEYWORD_ELSE },
    { .pattern = "^extern", .type = TOKEN_TYPE_KEYWORD_EXTERN },
    { .pattern = "^while", .type = TOKEN_TYPE_KEYWORD_WHILE },

    { .pattern = "^0x[a-fA-F\\d]+", .type = TOKEN_TYPE_NUMBER_HEX },
    { .pattern = "^0b[01]+", .type = TOKEN_TYPE_NUMBER_BIN },
    { .pattern = "^0o[0-7]+", .type = TOKEN_TYPE_NUMBER_OCT },
    { .pattern = "^\\d+", .type = TOKEN_TYPE_NUMBER_DEC },
    { .pattern = "^\"[^\"]*\"", .type = TOKEN_TYPE_STRING },
    { .pattern = "^'[^']'", .type = TOKEN_TYPE_CHAR },

    { .pattern = "^(?:void|uint|u8|u16|u32|u64)", .type = TOKEN_TYPE_TYPE },
    { .pattern = "^[_a-zA-Z][_a-zA-Z0-9]*", .type = TOKEN_TYPE_IDENTIFIER },

    { .pattern = "^\\.\\.\\.", .type = TOKEN_TYPE_TRIPLE_PERIOD },
    { .pattern = "^;", .type = TOKEN_TYPE_SEMI_COLON },
    { .pattern = "^==", .type = TOKEN_TYPE_EQUAL_EQUAL },
    { .pattern = "^=", .type = TOKEN_TYPE_EQUAL },
    { .pattern = "^\\+=", .type = TOKEN_TYPE_PLUS_EQUAL },
    { .pattern = "^-=", .type = TOKEN_TYPE_MINUS_EQUAL },
    { .pattern = "^\\*=", .type = TOKEN_TYPE_STAR_EQUAL },
    { .pattern = "^/=", .type = TOKEN_TYPE_SLASH_EQUAL },
    { .pattern = "^%=", .type = TOKEN_TYPE_PERCENTAGE_EQUAL },
    { .pattern = "^!=", .type = TOKEN_TYPE_NOT_EQUAL },
    { .pattern = "^!", .type = TOKEN_TYPE_NOT },
    { .pattern = "^\\/", .type = TOKEN_TYPE_SLASH },
    { .pattern = "^\\*", .type = TOKEN_TYPE_STAR },
    { .pattern = "^%", .type = TOKEN_TYPE_PERCENTAGE },
    { .pattern = "^\\+", .type = TOKEN_TYPE_PLUS },
    { .pattern = "^-", .type = TOKEN_TYPE_MINUS },
    { .pattern = "^>=", .type = TOKEN_TYPE_GREATER_EQUAL },
    { .pattern = "^>", .type = TOKEN_TYPE_GREATER },
    { .pattern = "^<=", .type = TOKEN_TYPE_LESS_EQUAL },
    { .pattern = "^<", .type = TOKEN_TYPE_LESS },
    { .pattern = "^\\(", .type = TOKEN_TYPE_PARENTHESES_LEFT },
    { .pattern = "^\\)", .type = TOKEN_TYPE_PARENTHESES_RIGHT },
    { .pattern = "^{", .type = TOKEN_TYPE_BRACE_LEFT },
    { .pattern = "^}", .type = TOKEN_TYPE_BRACE_RIGHT },
    { .pattern = "^,", .type = TOKEN_TYPE_COMMA },
    { .pattern = "^&", .type = TOKEN_TYPE_AMPERSAND }
};

static compiled_spec_t g_compiled_spec[sizeof(g_spec) / sizeof(spec_t)];
static bool g_spec_is_compiled = false;

static void print_pcre2_error(int error_code) {
    char error_message[120];
    pcre2_get_error_message(error_code, (uint8_t *) error_message, 120);
    printf("Error: %s\n", error_message);
}

static bool compile_spec() {
    for(size_t i = 0; i < sizeof(g_compiled_spec) / sizeof(compiled_spec_t); i++) {
        int error_code;
        PCRE2_SIZE error_offset;
        pcre2_code *code = pcre2_compile((const uint8_t *) g_spec[i].pattern, PCRE2_ZERO_TERMINATED, 0, &error_code, &error_offset, NULL);
        if(code == NULL) {
            print_pcre2_error(error_code);
            for(size_t j = 0; j < i; j++) pcre2_code_free(g_compiled_spec[j].pattern);
            return false;
        }
        g_compiled_spec[i].type = g_spec[i].type;
        g_compiled_spec[i].pattern = code;
    }
    g_spec_is_compiled = true;
    return true;
}

static token_t next_token(tokenizer_t *tokenizer) {
    if(tokenizer->cursor >= tokenizer->source->data_length) return (token_t) { .type = TOKEN_TYPE_EOF, .diag_loc = { .present = false } };

    const char *sub = tokenizer->source->data + tokenizer->cursor;
    size_t sub_length = tokenizer->source->data_length - tokenizer->cursor;
    for(size_t i = 0; i < sizeof(g_compiled_spec) / sizeof(compiled_spec_t); i++) {
        pcre2_match_data *md = pcre2_match_data_create(1, NULL);
        int match_count = pcre2_match(g_compiled_spec[i].pattern, (const uint8_t *) sub, sub_length, 0, 0, md, NULL);
        if(match_count <= 0) {
            pcre2_match_data_free(md);
            continue;
        }

        PCRE2_SIZE size;
        int error_code = pcre2_substring_length_bynumber(md, 0, &size);
        if(error_code != 0) {
            print_pcre2_error(error_code);
            pcre2_match_data_free(md);
            exit(EXIT_FAILURE);
        }
        pcre2_match_data_free(md);
        size_t offset = tokenizer->cursor;
        tokenizer->cursor += size;

        if(g_compiled_spec[i].type == TOKEN_TYPE_INTERNAL_NONE) return next_token(tokenizer);

        return (token_t) { .type = g_compiled_spec[i].type, .offset = offset, .length = size, .diag_loc = { .present = true, .offset = offset, .source = tokenizer->source } };
    }
    diag_error((diag_loc_t) { .present = true, .offset = tokenizer->cursor, .source = tokenizer->source }, "unexpected symbol `%c`", *sub);
}

tokenizer_t *tokenizer_make(source_t *source) {
    if(!g_spec_is_compiled && !compile_spec()) return NULL;

    tokenizer_t *tokenizer = malloc(sizeof(tokenizer_t));
    tokenizer->source = source;
    tokenizer->cursor = 0;
    tokenizer->lookahead = next_token(tokenizer);
    return tokenizer;
}

void tokenizer_free(tokenizer_t *tokenizer) {
    free(tokenizer);
}

token_t tokenizer_peek(tokenizer_t *tokenizer) {
    return tokenizer->lookahead;
}

token_t tokenizer_advance(tokenizer_t *tokenizer) {
    token_t token = tokenizer_peek(tokenizer);
    tokenizer->lookahead = next_token(tokenizer);
    return token;
}

bool tokenizer_is_eof(tokenizer_t *tokenizer) {
    return tokenizer->cursor >= tokenizer->source->data_length || tokenizer_peek(tokenizer).type == TOKEN_TYPE_EOF;
}