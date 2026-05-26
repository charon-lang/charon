#pragma once

#include "charon/lexer.h"
#include "common/text.h"

text_t *lexer_extract(charon_lexer_t *lexer, charon_lexer_token_t token);
