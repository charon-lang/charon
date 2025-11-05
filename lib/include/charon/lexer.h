#pragma once

#include "charon/element.h"

#include <stddef.h>

typedef struct charon_lexer charon_lexer_t;

charon_lexer_t *charon_lexer_make(charon_element_cache_t *element_cache, const char *data, size_t data_length);
void charon_lexer_destroy(charon_lexer_t *lexer);

const charon_element_inner_t *charon_lexer_peek(charon_lexer_t *lexer);
const charon_element_inner_t *charon_lexer_advance(charon_lexer_t *lexer);
bool charon_lexer_is_eof(charon_lexer_t *lexer);
