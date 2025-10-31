#pragma once

typedef enum charon_trivia_kind {
#define TRIVIA(ID, ...) CHARON_TRIVIA_KIND_##ID,
#include "charon/trivia.def"
#undef TRIVIA
} charon_trivia_kind_t;

const char *charon_trivia_kind_tostring(charon_trivia_kind_t kind);
