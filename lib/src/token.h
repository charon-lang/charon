#pragma once

#include "charon/token.h"

charon_token_t *token_make(charon_token_cache_t *cache, charon_token_kind_t kind, const char *text);
