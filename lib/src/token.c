#include "charon/token.h"

#include <stddef.h>

const char *charon_token_kind_tostring(charon_token_kind_t kind) {
    static const char *translations[] = {
        [CHARON_TOKEN_KIND_EOF] = "(eof)",
        [CHARON_TOKEN_KIND_UNKNOWN] = "(unknown)",
#define TOKEN(ID, NAME, ...) [CHARON_TOKEN_KIND_##ID] = NAME,
#include "charon/tokens.def"
#undef TOKEN
    };
    return translations[kind];
}

bool charon_token_kind_has_content(charon_token_kind_t kind) {
    static bool has_content[] = {
        [CHARON_TOKEN_KIND_EOF] = false,
        [CHARON_TOKEN_KIND_UNKNOWN] = false,
#define TOKEN(ID, _1, _2, HAS_CONTENT) [CHARON_TOKEN_KIND_##ID] = HAS_CONTENT,
#include "charon/tokens.def"
#undef TOKEN
    };
    return has_content[kind];
}
