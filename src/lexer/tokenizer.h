#pragma once
#include <stddef.h>
#include "token.h"
#include "../source.h"

typedef struct {
    source_t *source;
    size_t cursor;
    token_t lookahead;
} tokenizer_t;

tokenizer_t *tokenizer_make(source_t *source);
void tokenizer_free(tokenizer_t *tokenizer);

token_t tokenizer_advance(tokenizer_t *tokenizer);
token_t tokenizer_peek(tokenizer_t *tokenizer);
bool tokenizer_is_eof(tokenizer_t *tokenizer);