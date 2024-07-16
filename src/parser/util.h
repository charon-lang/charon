#pragma once

#include "lexer/tokenizer.h"
#include "lexer/token.h"
#include "ir/type.h"

#define UTIL_SRCLOC(TOKENIZER, TOKEN) ({ token_t token = (TOKEN); (source_location_t) { .source = (TOKENIZER)->source, .offset = token.offset, .length = token.size }; })

bool util_try_consume(tokenizer_t *tokenizer, token_kind_t kind);
token_t util_consume(tokenizer_t *tokenizer, token_kind_t kind);

char *util_text_make_from_token(tokenizer_t *tokenizer, token_t token);
int util_token_cmp(tokenizer_t *tokenizer, token_t token, const char *string);

ir_type_t *util_parse_type(tokenizer_t *tokenizer);
