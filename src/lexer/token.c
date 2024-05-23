#include "token.h"

static const char *g_token_translations[] = {
    "(internal_none)",
    "(internal_eof)",
    #define TOKEN(ID, NAME, _) [TOKEN_KIND_##ID] = "`" NAME "`",
    #include "tokens.def"
    #undef TOKEN
};

const char *token_kind_tostring(token_kind_t kind) {
    return g_token_translations[kind];
}