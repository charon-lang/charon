#include "lexer_pipeline.h"

#include "charon/lexer.h"
#include "charon/syntax/token.h"
#include "core/db.h"
#include "core/interner.h"
#include "core/store.h"
#include "lexer.h"
#include "syntax/element.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct lexer_pipeline {
    charon_lexer_t *lexer;
    db_t *db;

    store_handle_t lookahead;

    charon_lexer_token_t inner_lookahead;

    size_t cached_trivia_count;
    store_handle_t *cached_trivia;
};

static charon_lexer_token_t token_peek(lexer_pipeline_t *pipeline) {
    return pipeline->inner_lookahead;
}

static charon_lexer_token_t token_advance(lexer_pipeline_t *pipeline) {
    charon_lexer_token_t token = pipeline->inner_lookahead;
    pipeline->inner_lookahead = charon_lexer_advance(pipeline->lexer);
    return token;
}

static store_handle_t lexer_pipeline_next(lexer_pipeline_t *pipeline) {
    size_t trailing_trivia_count = 0;
    size_t leading_trivia_count = pipeline->cached_trivia_count;
    store_handle_t *trivia = pipeline->cached_trivia;

    pipeline->cached_trivia_count = 0;
    pipeline->cached_trivia = nullptr;

    charon_token_kind_t token_kind;
    store_handle_t token_text;

    charon_lexer_token_t token;
    while(true) {
        token = token_advance(pipeline);
        if(!token.is_trivia) break;

        assert(token.start != token.end);

        text_t *original_text = lexer_extract(pipeline->lexer, token);
        store_handle_t interned_text = interner_intern(pipeline->db->text_interner, original_text);
        free(original_text);

        trivia = reallocarray(trivia, ++leading_trivia_count, sizeof(interned_text));
        trivia[leading_trivia_count - 1] = element_inner_make_trivia(pipeline->db, token.kind.trivia, interned_text);
    }

    assert(!token.is_trivia);

    text_t *original_text = lexer_extract(pipeline->lexer, token);
    store_handle_t interned_text = interner_intern(pipeline->db->text_interner, original_text);
    free(original_text);

    token_kind = token.kind.token;
    token_text = interned_text;

    while(true) {
        token = token_peek(pipeline);
        if(!token.is_trivia) {
            if(token.kind.token == CHARON_TOKEN_KIND_EOF) goto consume_trailing;
            break;
        }

        token = token_advance(pipeline);

        assert(token.kind.token != CHARON_TOKEN_KIND_EOF);

        text_t *original_text = lexer_extract(pipeline->lexer, token);
        store_handle_t interned_text = interner_intern(pipeline->db->text_interner, original_text);
        free(original_text);

        pipeline->cached_trivia = reallocarray(pipeline->cached_trivia, ++pipeline->cached_trivia_count, sizeof(store_handle_t));
        pipeline->cached_trivia[pipeline->cached_trivia_count - 1] = element_inner_make_trivia(pipeline->db, token.kind.trivia, interned_text);

        if(token.kind.trivia == CHARON_TRIVIA_KIND_NEWLINE) {
        consume_trailing:
            trailing_trivia_count = pipeline->cached_trivia_count;
            trivia = reallocarray(trivia, leading_trivia_count + trailing_trivia_count, sizeof(store_handle_t));
            for(size_t i = 0; i < trailing_trivia_count; i++) trivia[leading_trivia_count + i] = pipeline->cached_trivia[i];

            pipeline->cached_trivia_count = 0;
            pipeline->cached_trivia = nullptr;
            break;
        }
    }

    store_handle_t element = element_inner_make_token(pipeline->db, token_kind, token_text, leading_trivia_count, trailing_trivia_count, trivia);
    free(trivia);
    return element;
}

lexer_pipeline_t *lexer_pipeline_make(db_t *db, charon_lexer_t *lexer) {
    lexer_pipeline_t *pipeline = malloc(sizeof(lexer_pipeline_t));
    pipeline->db = db;
    pipeline->lexer = lexer;
    pipeline->cached_trivia_count = 0;
    pipeline->cached_trivia = nullptr;

    pipeline->inner_lookahead = charon_lexer_advance(pipeline->lexer);
    pipeline->lookahead = lexer_pipeline_next(pipeline);

    return pipeline;
}

void lexer_pipeline_destroy(lexer_pipeline_t *pipeline) {
    if(pipeline->cached_trivia != nullptr) free(pipeline->cached_trivia);
    free(pipeline);
}

charon_token_kind_t lexer_pipeline_peek(lexer_pipeline_t *pipeline) {
    return ((const element_inner_t *) store_get(pipeline->db->store, pipeline->lookahead))->token.kind;
}

store_handle_t lexer_pipeline_advance(lexer_pipeline_t *pipeline) {
    store_handle_t element = pipeline->lookahead;
    pipeline->lookahead = lexer_pipeline_next(pipeline);
    return element;
}

bool lexer_pipeline_is_eof(lexer_pipeline_t *pipeline) {
    return lexer_pipeline_peek(pipeline) == CHARON_TOKEN_KIND_EOF;
}
