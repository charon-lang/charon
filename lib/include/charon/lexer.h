#pragma once

#include "charon/syntax/token.h"
#include "charon/syntax/trivia.h"

#include <stddef.h>
#include <stdint.h>

typedef struct charon_lexer charon_lexer_t;

typedef struct {
    size_t start, end;
    bool is_trivia;
    union {
        charon_token_kind_t token;
        charon_trivia_kind_t trivia;
    } kind;
} charon_lexer_token_t;

charon_lexer_t *charon_lexer_new(const uint8_t *data, size_t data_size);
void charon_lexer_destroy(charon_lexer_t *lexer);

charon_lexer_token_t charon_lexer_advance(charon_lexer_t *lexer);
