#pragma once

#include "lexer/token.h"
#include "lib/source.h"

#include <stddef.h>

typedef struct {
    source_t *source;
    size_t cursor;
    token_t lookahead;
} tokenizer_t;

tokenizer_t tokenizer_init(source_t *source);
token_t tokenizer_peek(tokenizer_t *tokenizer);
token_t tokenizer_advance(tokenizer_t *tokenizer);
bool tokenizer_is_eof(tokenizer_t *tokenizer);
