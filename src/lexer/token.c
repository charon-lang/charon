#include "token.h"

static const char *g_token_translations[] = {
#define TOKEN(ID, NAME) [TOKEN_TYPE_##ID] = "`" NAME "`",
#include "tokens.def"
#undef TOKEN
};

const char *token_type_name(token_type_t type) {
    return g_token_translations[type];
}

bool token_match(token_t token, size_t count, ...) {
    va_list list;
    va_start(list, count);
    bool match = token_match_list(token, count, list);
    va_end(list);
    return match;
}

bool token_match_list(token_t token, size_t count, va_list list) {
    for(size_t i = 0; i < count; i++) {
        token_type_t type = va_arg(list, token_type_t);
        if(type == token.type) return true;
    }
    return false;
}