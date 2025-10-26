#pragma once

typedef enum charon_token_kind {
    CHARON_TOKEN_KIND_UNKNOWN,
    CHARON_TOKEN_KIND_EOF,
#define TOKEN(ID, ...) CHARON_TOKEN_KIND_##ID,
#include "charon/tokens.def"
#undef TOKEN
} charon_token_kind_t;

typedef struct charon_token {
    charon_token_kind_t kind;
    const char *text;
} charon_token_t;

const char *charon_token_kind_tostring(charon_token_kind_t kind);
bool charon_token_kind_has_content(charon_token_kind_t kind);
