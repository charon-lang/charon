#include "tokenizer.h"

#include "lexer/spec.h"
#include "lib/diag.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
    spec_ensure();
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