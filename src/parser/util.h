#pragma once

#include "lexer/tokenizer.h"
#include "lexer/token.h"
#include "hlir/type.h"
#include "hlir/attribute.h"

#include <stdint.h>

source_location_t util_loc(tokenizer_t *tokenizer, token_t token);

bool util_try_consume(tokenizer_t *tokenizer, token_kind_t kind);
token_t util_consume(tokenizer_t *tokenizer, token_kind_t kind);

char *util_text_make_from_token(tokenizer_t *tokenizer, token_t token);
int util_token_cmp(tokenizer_t *tokenizer, token_t token, const char *string);

uintmax_t util_number_make_from_token(tokenizer_t *tokenizer, token_t token);

hlir_type_t *util_parse_type(tokenizer_t *tokenizer);

hlir_type_function_t *util_parse_function_type(tokenizer_t *tokenizer, const char ***argument_names);

hlir_attribute_list_t util_parse_hlir_attributes(tokenizer_t *tokenizer);