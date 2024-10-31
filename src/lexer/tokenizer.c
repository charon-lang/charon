#include "tokenizer.h"

#include "lib/diag.h"
#include "lib/log.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pcre2.h>

#define SPEC_SIZE (sizeof(g_uncompiled_spec) / sizeof(uncompiled_entry_t))

typedef struct {
    token_kind_t kind;
    size_t size;
} spec_match_t;

typedef struct {
    token_kind_t token_kind;
    pcre2_code *pattern;
} entry_t;

typedef struct {
    token_kind_t kind;
    const char *pattern;
} uncompiled_entry_t;

static uncompiled_entry_t g_uncompiled_spec[] = {
    { .kind = TOKEN_KIND_INTERNAL_NONE, .pattern = "^\\s+" }, // Whitespace
    { .kind = TOKEN_KIND_INTERNAL_NONE, .pattern = "^//.*" }, // Single line comment
    { .kind = TOKEN_KIND_INTERNAL_NONE, .pattern = "^/\\*[\\s\\S]*?\\*/" }, // Multi line comment
    #define TOKEN(ID, _, PATTERN) { .kind = TOKEN_KIND_##ID, .pattern = PATTERN },
    #include "tokens.def"
    #undef TOKEN
};

static bool g_spec_compiled = false;
static entry_t g_spec[SPEC_SIZE];

static void spec_compile() {
    for(size_t i = 0; i < SPEC_SIZE; i++) {
        int error_code;
        PCRE2_SIZE error_offset;
        pcre2_code *code = pcre2_compile((const uint8_t *) g_uncompiled_spec[i].pattern, PCRE2_ZERO_TERMINATED, 0, &error_code, &error_offset, NULL);
        if(code == NULL) {
            char error_message[120];
            pcre2_get_error_message(error_code, (uint8_t *) error_message, 120);
            log_fatal("failed compiling pattern '%s' (%s)", g_uncompiled_spec[i].pattern, error_message);
        }
        g_spec[i] = (entry_t) { .token_kind = g_uncompiled_spec[i].kind, .pattern = code };
    }
    g_spec_compiled = true;
}

static spec_match_t spec_match(const char *string, size_t string_length) {
    pcre2_match_data *md = pcre2_match_data_create(1, NULL);
    for(size_t i = 0; i < SPEC_SIZE; i++) {
        int match_count = pcre2_match(g_spec[i].pattern, (const uint8_t *) string, string_length, 0, 0, md, NULL);
        if(match_count <= 0) continue;

        PCRE2_SIZE size;
        int error_code = pcre2_substring_length_bynumber(md, 0, &size);
        if(error_code != 0) {
            char error_message[120];
            pcre2_get_error_message(error_code, (uint8_t *) error_message, 120);
            log_fatal("error during spec matching '%s' (%s)", g_uncompiled_spec[i].pattern, error_message);
        }
        pcre2_match_data_free(md);
        return (spec_match_t) { .kind = g_spec[i].token_kind, .size = size };
    }
    pcre2_match_data_free(md);
    return (spec_match_t) { .kind = TOKEN_KIND_INTERNAL_NONE, .size = 0 };
}

static token_t next_token(tokenizer_t *tokenizer) {
    if(tokenizer_is_eof(tokenizer)) return (token_t) { .kind = TOKEN_KIND_INTERNAL_EOF };

    size_t offset = tokenizer->cursor;

    const char *sub = tokenizer->source->data_buffer + offset;
    size_t sub_length = tokenizer->source->data_buffer_size - offset;
    spec_match_t match = spec_match(sub, sub_length);
    tokenizer->cursor += match.size;

    if(match.size == 0) diag_error((source_location_t) { .source = tokenizer->source, .offset = tokenizer->cursor, .length = 1 }, "unexpected symbol `%c`", *sub);
    if(match.kind == TOKEN_KIND_INTERNAL_NONE) return next_token(tokenizer);

    return (token_t) { .kind = match.kind, .offset = offset, .size = match.size };
}

tokenizer_t *tokenizer_make(source_t *source) {
    if(!g_spec_compiled) spec_compile();

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
    return tokenizer->cursor >= tokenizer->source->data_buffer_size;
}