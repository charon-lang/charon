#pragma once

typedef enum charon_token_kind {
    CHARON_TOKEN_KIND_UNKNOWN,
    CHARON_TOKEN_KIND_EOF,
#define TOKEN(ID, ...) CHARON_TOKEN_KIND_##ID,
#include "charon/tokens.def"
#undef TOKEN
} charon_token_kind_t;

const char *charon_token_kind_tostring(charon_token_kind_t kind);
