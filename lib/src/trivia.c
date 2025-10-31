#include "charon/trivia.h"

#include <stddef.h>

const char *charon_trivia_kind_tostring(charon_trivia_kind_t kind) {
    static const char *translations[] = {
#define TRIVIA(ID, NAME, ...) [CHARON_TRIVIA_KIND_##ID] = NAME,
#include "charon/trivia.def"
#undef TRIVIA
    };
    return translations[kind];
}
