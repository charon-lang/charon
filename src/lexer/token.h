#pragma once

#include <stdarg.h>
#include <stddef.h>

typedef enum {
    TOKEN_KIND_INTERNAL_NONE,
    TOKEN_KIND_INTERNAL_EOF,
    #define TOKEN(ID, _, __) TOKEN_KIND_##ID,
    #include "tokens.def"
    #undef TOKEN
} token_kind_t;

typedef struct {
    token_kind_t kind;
    size_t offset, size;
} token_t;

const char *token_kind_tostring(token_kind_t kind);