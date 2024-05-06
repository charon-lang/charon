#pragma once
#include <stdarg.h>
#include <stddef.h>
#include "../diag.h"

typedef enum {
#define TOKEN(ID, _) TOKEN_TYPE_##ID,
#include "tokens.def"
#undef TOKEN
} token_type_t;

typedef struct {
    token_type_t type;
    size_t offset, length;
    diag_loc_t diag_loc;
} token_t;

const char *token_type_name(token_type_t type);

bool token_match(token_t token, size_t count, ...);
bool token_match_list(token_t token, size_t count, va_list list);