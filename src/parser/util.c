#include "util.h"

#include "lib/source.h"
#include "lib/diag.h"

bool util_try_consume(tokenizer_t *tokenizer, token_kind_t kind) {
    if(tokenizer_peek(tokenizer).kind == kind) {
        tokenizer_advance(tokenizer);
        return true;
    }
    return false;
}

token_t util_consume(tokenizer_t *tokenizer, token_kind_t kind) {
    token_t token = tokenizer_advance(tokenizer);
    if(token.kind == kind) return token;
    diag_error(UTIL_MAKE_SRCLOC(tokenizer, token), "expected %s got %s", token_kind_tostring(kind), token_kind_tostring(token.kind));
}