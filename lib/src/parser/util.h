#pragma once

#include "ast/attribute.h"
#include "ast/type.h"
#include "lexer/token.h"
#include "lexer/tokenizer.h"
#include "lib/diag.h"

#include <stdint.h>

source_location_t util_loc(tokenizer_t *tokenizer, token_t token);

bool util_try_consume(tokenizer_t *tokenizer, token_kind_t kind);
token_t util_consume(tokenizer_t *tokenizer, token_kind_t kind);

char *util_text_make_from_token_inset(tokenizer_t *tokenizer, token_t token, size_t inset);
char *util_text_make_from_token(tokenizer_t *tokenizer, token_t token);

int util_token_cmp(tokenizer_t *tokenizer, token_t token, const char *string);

uintmax_t util_number_make_from_token(tokenizer_t *tokenizer, token_t token);

ast_type_t *util_parse_type(tokenizer_t *tokenizer);

ast_type_function_t *util_parse_function_type(tokenizer_t *tokenizer, const char ***argument_names);

ast_attribute_list_t util_parse_ast_attributes(tokenizer_t *tokenizer);
