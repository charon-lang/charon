#include "charon/lexer.h"

#include "charon/element.h"
#include "charon/token.h"
#include "common/fatal.h"
#include "source.h"

#include <assert.h>
#include <pcre2.h>
#include <stdlib.h>
#include <string.h>

#define SPEC_SIZE (sizeof(g_uncompiled_spec) / sizeof(uncompiled_entry_t))

typedef struct {
    charon_token_kind_t kind;
    size_t size;
} spec_match_t;

typedef struct {
    charon_token_kind_t token_kind;
    pcre2_code *pattern;
} entry_t;

typedef struct {
    charon_token_kind_t kind;
    const char *pattern;
} uncompiled_entry_t;

struct charon_lexer {
    charon_element_cache_t *cache;
    charon_source_t *source;
    charon_element_t *lookahead;
    size_t cursor;
};

static uncompiled_entry_t g_uncompiled_spec[] = {
#define TOKEN(ID, _1, PATTERN, _2) { .kind = CHARON_TOKEN_KIND_##ID, .pattern = PATTERN },
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
            fatal("error during spec matching '%s' (%s)", g_uncompiled_spec[i].pattern, error_message);
        }
        pcre2_match_data_free(md);
        return (spec_match_t) { .kind = g_spec[i].token_kind, .size = size };
    }
    pcre2_match_data_free(md);
    return (spec_match_t) { .kind = CHARON_TOKEN_KIND_UNKNOWN, .size = 0 };
}

static charon_element_t *next_token(charon_lexer_t *lexer) {
    while(true) {
        if(charon_lexer_is_eof(lexer)) return charon_element_make_token(lexer->cache, CHARON_TOKEN_KIND_EOF, nullptr);

        size_t offset = lexer->cursor;

        const char *sub = lexer->source->text + offset;
        size_t sub_length = lexer->source->text_length - offset;

        spec_match_t match = spec_match(sub, sub_length);
        if(match.size == 0) {
            // TODO:
            // diag((charon_diag_t) { .type = DIAG_TYPE__UNEXPECTED_SYMBOL, .location = SOURCE_LOC(lexer->source, lexer->cursor, 1) });
            lexer->cursor++;
            continue;
        }

        lexer->cursor += match.size;

        char *text = nullptr;
        if(charon_token_kind_has_content(match.kind)) {
            text = malloc(match.size + 1);
            strncpy(text, &lexer->source->text[offset], match.size);
            text[match.size] = '\0';
        }
        charon_element_t *el = charon_element_make_token(lexer->cache, match.kind, text);
        if(text != nullptr) free(text);
        return el;
    }
}

charon_lexer_t *charon_lexer_make(charon_element_cache_t *cache, charon_source_t *source) {
    assert(cache != nullptr);
    assert(source != nullptr);

    if(!g_spec_compiled) spec_compile();

    charon_lexer_t *lexer = malloc(sizeof(charon_lexer_t));
    lexer->cache = cache;
    lexer->source = source;
    lexer->cursor = 0;
    lexer->lookahead = next_token(lexer);
    return lexer;
}

void charon_lexer_destroy(charon_lexer_t *lexer) {
    assert(lexer != nullptr);

    free(lexer);
}

charon_element_t *charon_lexer_peek(charon_lexer_t *lexer) {
    return lexer->lookahead;
}

charon_element_t *charon_lexer_advance(charon_lexer_t *lexer) {
    charon_element_t *el = charon_lexer_peek(lexer);
    lexer->lookahead = next_token(lexer);
    return el;
}

bool charon_lexer_is_eof(charon_lexer_t *lexer) {
    return lexer->cursor >= lexer->source->text_length;
}
