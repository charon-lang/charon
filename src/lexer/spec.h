#pragma once

#include "lexer/token.h"

#include <stddef.h>

typedef struct {
    token_kind_t kind;
    size_t size;
} spec_match_t;

void spec_ensure();
spec_match_t spec_match(const char *string, size_t string_length);