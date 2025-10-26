#include "charon/lexer.h"

#include "charon/token.h"
#include "fatal.h"
#include "source.h"

#include <assert.h>
#include <pcre2.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SPEC_SIZE (sizeof(g_uncompiled_spec) / sizeof(uncompiled_entry_t))

typedef struct {
    charon_token_kind_t kind;
    size_t size;
    bool is_newline;
} spec_match_t;

typedef struct {
    charon_token_kind_t token_kind;
    bool is_newline;
    pcre2_code *pattern;
} entry_t;

typedef struct {
    charon_token_kind_t kind;
    bool is_newline;
    const char *pattern;
} uncompiled_entry_t;

struct charon_lexer {
    charon_source_t *source;
    charon_lexer_token_t lookahead;
    size_t cursor;
    size_t line, column;
};

static uncompiled_entry_t g_uncompiled_spec[] = {
#define TOKEN(ID, _1, PATTERN, _2, IS_NEWLINE) { .kind = CHARON_TOKEN_KIND_##ID, .pattern = PATTERN, .is_newline = IS_NEWLINE },
#include "charon/tokens.def"
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
            fatal("failed compiling pattern '%s' (%s)", g_uncompiled_spec[i].pattern, error_message);
        }
        g_spec[i] = (entry_t) { .token_kind = g_uncompiled_spec[i].kind, .is_newline = g_uncompiled_spec[i].is_newline, .pattern = code };
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
            fatal("error during spec matching '%s' (%s)", g_uncompiled_spec[i].pattern, error_message);
        }
        pcre2_match_data_free(md);
        return (spec_match_t) { .kind = g_spec[i].token_kind, .size = size, .is_newline = g_spec[i].is_newline };
    }
    pcre2_match_data_free(md);
    return (spec_match_t) { .kind = CHARON_TOKEN_KIND_UNKNOWN, .size = 0, .is_newline = false };
}

static charon_lexer_token_t next_token(charon_lexer_t *lexer) {
    while(true) {
        if(charon_lexer_is_eof(lexer)) return (charon_lexer_token_t) { .kind = CHARON_TOKEN_KIND_EOF };

        size_t offset = lexer->cursor;
        size_t line = lexer->line;
        size_t column = lexer->column;

        const char *sub = lexer->source->text + offset;
        size_t sub_length = lexer->source->text_length - offset;

        spec_match_t match = spec_match(sub, sub_length);
        if(match.size == 0) {
            // TODO: diag((charon_diag_t) { .type = DIAG_TYPE__UNEXPECTED_SYMBOL, .location = SOURCE_LOC(lexer->source, lexer->cursor, 1) });
            lexer->cursor++;
            lexer->column++;
            continue;
        }
        lexer->cursor += match.size;

        lexer->column += match.size;
        if(match.is_newline) {
            lexer->line++;
            lexer->column = 0;
        }

        return (charon_lexer_token_t) { .kind = match.kind, .offset = offset, .length = match.size, .line = line, .column = column };
    }
}

charon_lexer_t *charon_lexer_make(charon_source_t *source) {
    assert(source != nullptr);

    if(!g_spec_compiled) spec_compile();

    charon_lexer_t *lexer = malloc(sizeof(charon_lexer_t));
    lexer->source = source;
    lexer->cursor = 0;
    lexer->line = 0;
    lexer->column = 0;
    lexer->lookahead = next_token(lexer);
    return lexer;
}

void charon_lexer_destroy(charon_lexer_t *lexer) {
    assert(lexer != nullptr);

    free(lexer);
}

charon_lexer_token_t charon_lexer_peek(charon_lexer_t *lexer) {
    return lexer->lookahead;
}

charon_lexer_token_t charon_lexer_advance(charon_lexer_t *lexer) {
    charon_lexer_token_t token = charon_lexer_peek(lexer);
    lexer->lookahead = next_token(lexer);
    return token;
}

bool charon_lexer_is_eof(charon_lexer_t *lexer) {
    return lexer->cursor >= lexer->source->text_length;
}
