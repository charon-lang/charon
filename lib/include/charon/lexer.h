#pragma once

#include "charon/source.h"
#include "charon/token.h"

#include <stddef.h>

typedef struct charon_lexer charon_lexer_t;

typedef struct {
    charon_token_kind_t kind;
    size_t offset, length;
    size_t line, column;
} charon_lexer_token_t;

charon_lexer_t *charon_lexer_make(charon_source_t *source);
void charon_lexer_destroy(charon_lexer_t *lexer);

charon_lexer_token_t charon_lexer_peek(charon_lexer_t *lexer);
charon_lexer_token_t charon_lexer_advance(charon_lexer_t *lexer);
bool charon_lexer_is_eof(charon_lexer_t *lexer);
