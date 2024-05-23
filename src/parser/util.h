#pragma once

#include "lexer/tokenizer.h"
#include "lexer/token.h"

#define UTIL_MAKE_SRCLOC(TOKENIZER, TOKEN) ((source_location_t) { .source = (TOKENIZER)->source, .offset = (TOKEN).offset })

bool util_try_consume(tokenizer_t *tokenizer, token_kind_t kind);
token_t util_consume(tokenizer_t *tokenizer, token_kind_t kind);