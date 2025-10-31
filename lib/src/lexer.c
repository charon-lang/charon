#include "charon/lexer.h"

#include "charon/element.h"
#include "charon/source.h"
#include "charon/token.h"
#include "charon/trivia.h"
#include "common/fatal.h"
#include "source.h"

#include <assert.h>
#include <pcre2.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SPEC_SIZE (sizeof(g_uncompiled_spec) / sizeof(uncompiled_entry_t))

typedef struct {
    bool is_trivia;
    union {
        charon_token_kind_t token_kind;
        charon_trivia_kind_t trivia_kind;
    };
} lexer_token_kind_t;

typedef struct {
    const char *pattern;
    lexer_token_kind_t kind;
} uncompiled_entry_t;

typedef struct {
    pcre2_code *pattern;
    lexer_token_kind_t kind;
} entry_t;

typedef struct {
    lexer_token_kind_t kind;
    size_t size;
} spec_match_t;

struct charon_lexer {
    charon_source_t *source;
    charon_element_cache_t *cache;

    size_t cursor;
    bool is_eof;

    charon_element_inner_t *lookahead;

    size_t cached_trivia_count;
    charon_element_inner_t **cached_trivia;
};

static uncompiled_entry_t g_uncompiled_spec[] = {
#define TRIVIA(ID, _1, PATTERN) { .kind.is_trivia = true, .kind.trivia_kind = CHARON_TRIVIA_KIND_##ID, .pattern = PATTERN },
#include "charon/trivia.def"
#undef TRIVIA
#define TOKEN(ID, _1, PATTERN) { .kind.is_trivia = false, .kind.token_kind = CHARON_TOKEN_KIND_##ID, .pattern = PATTERN },
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
        g_spec[i] = (entry_t) { .kind = g_uncompiled_spec[i].kind, .pattern = code };
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
        return (spec_match_t) { .kind = g_spec[i].kind, .size = size };
    }
    pcre2_match_data_free(md);
    return (spec_match_t) { .kind.is_trivia = false, .kind.token_kind = CHARON_TOKEN_KIND_UNKNOWN, .size = 0 };
}

static spec_match_t next_match(charon_lexer_t *lexer, spec_match_t *match) {
    const char *sub = lexer->source->text + lexer->cursor;
    size_t sub_length = lexer->source->text_length - lexer->cursor;
    return spec_match(sub, sub_length);
}

static char *lexer_extract(charon_lexer_t *lexer, size_t length) {
    char *str = malloc(length + 1);
    memcpy(str, &lexer->source->text[lexer->cursor], length);
    str[length] = '\0';
    lexer->cursor += length;
    return str;
}

static bool is_eof(charon_lexer_t *lexer) {
    return lexer->cursor >= lexer->source->text_length;
}

static charon_element_inner_t *next(charon_lexer_t *lexer) {
    size_t trailing_trivia_count = 0;
    size_t leading_trivia_count = lexer->cached_trivia_count;
    charon_element_inner_t **trivia = lexer->cached_trivia;

    lexer->cached_trivia_count = 0;
    lexer->cached_trivia = nullptr;

    charon_token_kind_t token_kind;
    char *token_text;

    spec_match_t match;
    while(true) {
        if(is_eof(lexer)) {
            lexer->is_eof = true;
            token_kind = CHARON_TOKEN_KIND_EOF;
            token_text = nullptr;
            goto exit;
        }

        match = next_match(lexer, &match);
        if(match.size == 0 || !match.kind.is_trivia) break;

        char *text = lexer_extract(lexer, match.size);
        trivia = reallocarray(trivia, ++leading_trivia_count, sizeof(charon_element_inner_t *));
        trivia[leading_trivia_count - 1] = charon_element_inner_make_trivia(lexer->cache, match.kind.trivia_kind, text);
        free(text);
    }

    assert(match.size == 0 || !match.kind.is_trivia);

    if(match.size == 0) {
        token_kind = CHARON_TOKEN_KIND_UNKNOWN;
        token_text = lexer_extract(lexer, 1);
    } else {
        token_kind = match.kind.token_kind;
        token_text = lexer_extract(lexer, match.size);
    }

    while(true) {
        if(is_eof(lexer)) {
            lexer->is_eof = true;
            goto consume_trailing;
        }

        match = next_match(lexer, &match);
        if(match.size == 0 || !match.kind.is_trivia) break;

        char *text = lexer_extract(lexer, match.size);
        lexer->cached_trivia = reallocarray(lexer->cached_trivia, ++lexer->cached_trivia_count, sizeof(charon_element_inner_t *));
        lexer->cached_trivia[lexer->cached_trivia_count - 1] = charon_element_inner_make_trivia(lexer->cache, match.kind.trivia_kind, text);
        free(text);

        if(match.kind.trivia_kind == CHARON_TRIVIA_KIND_NEWLINE) {
        consume_trailing:
            trailing_trivia_count = lexer->cached_trivia_count;
            trivia = reallocarray(trivia, leading_trivia_count + trailing_trivia_count, sizeof(charon_element_inner_t *));
            for(size_t i = 0; i < trailing_trivia_count; i++) trivia[leading_trivia_count + i] = lexer->cached_trivia[i];

            lexer->cached_trivia_count = 0;
            lexer->cached_trivia = nullptr;
            break;
        }
    }

exit:
    charon_element_inner_t *element = charon_element_inner_make_token(lexer->cache, token_kind, token_text, leading_trivia_count, trailing_trivia_count, trivia);

    free(token_text);
    free(trivia);

    return element;
}

charon_lexer_t *charon_lexer_make(charon_element_cache_t *element_cache, charon_source_t *source) {
    assert(element_cache != nullptr);
    assert(source != nullptr);

    if(!g_spec_compiled) spec_compile();

    charon_lexer_t *lexer = malloc(sizeof(charon_lexer_t));
    lexer->cache = element_cache;
    lexer->source = source;
    lexer->cursor = 0;
    lexer->is_eof = false;
    lexer->cached_trivia = nullptr;
    lexer->cached_trivia_count = 0;
    lexer->lookahead = next(lexer);
    return lexer;
}

void charon_lexer_destroy(charon_lexer_t *lexer) {
    assert(lexer != nullptr);

    free(lexer->cached_trivia);
    free(lexer);
}

charon_element_inner_t *charon_lexer_peek(charon_lexer_t *lexer) {
    return lexer->lookahead;
}

charon_element_inner_t *charon_lexer_advance(charon_lexer_t *lexer) {
    charon_element_inner_t *element = charon_lexer_peek(lexer);
    if(!charon_lexer_is_eof(lexer)) lexer->lookahead = next(lexer);
    return element;
}

bool charon_lexer_is_eof(charon_lexer_t *lexer) {
    return lexer->is_eof;
}
