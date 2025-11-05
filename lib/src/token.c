#include "charon/token.h"

#include <stddef.h>

const char *charon_token_kind_tostring(charon_token_kind_t kind) {
    static const char *translations[] = {
        [CHARON_TOKEN_KIND_UNKNOWN] = "(unknown)",
#define TOKEN(ID, NAME, ...) [CHARON_TOKEN_KIND_##ID] = NAME,
#include "charon/tokens.def"
#undef TOKEN
        [CHARON_TOKEN_KIND_EOF] = "(eof)",
    };
    return translations[kind];
}
