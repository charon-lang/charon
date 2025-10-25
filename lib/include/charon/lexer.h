#pragma once

#include "charon/diag.h"
#include "charon/source.h"
#include "charon/token.h"

typedef struct charon_lexer charon_lexer_t;

charon_lexer_t *charon_lexer_make(charon_token_cache_t *token_cache, charon_source_t *source);
void charon_lexer_destroy(charon_lexer_t *lexer);

charon_token_t *charon_lexer_peek(charon_lexer_t *lexer);
charon_token_t *charon_lexer_advance(charon_lexer_t *lexer);
bool charon_lexer_is_eof(charon_lexer_t *lexer);
