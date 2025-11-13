#include "charon/lexer.h"

#include "charon/element.h"
#include "charon/token.h"
#include "charon/trivia.h"
#include "common/fatal.h"
#include "common/utf8.h"

#include <assert.h>
#include <pcre2.h>
#include <stddef.h>
#include <stdint.h>
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
    charon_element_cache_t *cache;

    const charon_utf8_text_t *text;

    size_t cursor;
    bool is_eof;

    const charon_element_inner_t *lookahead;

    size_t cached_trivia_count;
    const charon_element_inner_t **cached_trivia;
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
        pcre2_code *code = pcre2_compile((const uint8_t *) g_uncompiled_spec[i].pattern, PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP, &error_code, &error_offset, nullptr);
        if(code == nullptr) {
            char error_message[120];
            pcre2_get_error_message(error_code, (uint8_t *) error_message, 120);
            fatal("failed compiling pattern '%s' (%s)", g_uncompiled_spec[i].pattern, error_message);
        }
        g_spec[i] = (entry_t) { .kind = g_uncompiled_spec[i].kind, .pattern = code };
    }
    g_spec_compiled = true;
}

static spec_match_t spec_match(utf8_slice_t slice) {
    pcre2_match_data *md = pcre2_match_data_create(1, nullptr);
    for(size_t i = 0; i < SPEC_SIZE; i++) {
        int match_count = pcre2_match(g_spec[i].pattern, &slice.text->data[slice.start_index], slice.size, 0, PCRE2_NO_UTF_CHECK, md, NULL);
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

static spec_match_t next_match(charon_lexer_t *lexer) {
    return spec_match(utf8_slice(lexer->text, lexer->cursor, lexer->text->size));
}

static bool is_eof(charon_lexer_t *lexer) {
    return lexer->cursor >= lexer->text->size;
}

static charon_utf8_text_t *lexer_extract(charon_lexer_t *lexer, size_t length) {
    utf8_slice_t slice = utf8_slice(lexer->text, lexer->cursor, length);
    lexer->cursor += length;
    return utf8_slice_copy(slice);
}

static const charon_element_inner_t *next(charon_lexer_t *lexer) {
    size_t trailing_trivia_count = 0;
    size_t leading_trivia_count = lexer->cached_trivia_count;
    const charon_element_inner_t **trivia = lexer->cached_trivia;

    lexer->cached_trivia_count = 0;
    lexer->cached_trivia = nullptr;

    charon_token_kind_t token_kind;
    charon_utf8_text_t *token_text;

    spec_match_t match;
    while(true) {
        if(is_eof(lexer)) {
            lexer->is_eof = true;
            token_kind = CHARON_TOKEN_KIND_EOF;
            token_text = nullptr;
            goto exit;
        }

        match = next_match(lexer);
        if(match.size == 0 || !match.kind.is_trivia) break;

        charon_utf8_text_t *text = lexer_extract(lexer, match.size);
        trivia = reallocarray(trivia, ++leading_trivia_count, sizeof(charon_element_inner_t *));
        trivia[leading_trivia_count - 1] = charon_element_inner_make_trivia(lexer->cache, match.kind.trivia_kind, text);
    }

    assert(match.size == 0 || !match.kind.is_trivia);

    if(match.size == 0) {
        size_t char_length = charon_utf8_lead_width(lexer->text->data[lexer->cursor]);
        if(char_length > lexer->text->size - lexer->cursor) fatal("lexer encountered truncated character sequence at the end of the file");

        token_kind = CHARON_TOKEN_KIND_UNKNOWN;
        token_text = lexer_extract(lexer, char_length);
    } else {
        token_kind = match.kind.token_kind;
        token_text = lexer_extract(lexer, match.size);
    }

    while(true) {
        if(is_eof(lexer)) goto consume_trailing;

        match = next_match(lexer);
        if(match.size == 0 || !match.kind.is_trivia) break;

        charon_utf8_text_t *text = lexer_extract(lexer, match.size);
        lexer->cached_trivia = reallocarray(lexer->cached_trivia, ++lexer->cached_trivia_count, sizeof(charon_element_inner_t *));
        lexer->cached_trivia[lexer->cached_trivia_count - 1] = charon_element_inner_make_trivia(lexer->cache, match.kind.trivia_kind, text);

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
    const charon_element_inner_t *element = charon_element_inner_make_token(lexer->cache, token_kind, token_text, leading_trivia_count, trailing_trivia_count, trivia);

    free(trivia);

    return element;
}

charon_lexer_t *charon_lexer_make(charon_element_cache_t *element_cache, const charon_utf8_text_t *text) {
    assert(element_cache != nullptr);

    if(!g_spec_compiled) spec_compile();

    charon_lexer_t *lexer = malloc(sizeof(charon_lexer_t));
    lexer->cache = element_cache;
    lexer->text = text;
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

const charon_element_inner_t *charon_lexer_peek(charon_lexer_t *lexer) {
    return lexer->lookahead;
}

const charon_element_inner_t *charon_lexer_advance(charon_lexer_t *lexer) {
    const charon_element_inner_t *element = charon_lexer_peek(lexer);
    if(!charon_lexer_is_eof(lexer)) lexer->lookahead = next(lexer);
    return element;
}

bool charon_lexer_is_eof(charon_lexer_t *lexer) {
    return lexer->is_eof;
}
