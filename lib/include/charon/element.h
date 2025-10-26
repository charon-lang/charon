#pragma once

#include "charon/memory.h"
#include "charon/node.h"
#include "charon/token.h"

#include <stddef.h>

typedef struct charon_element_cache charon_element_cache_t;
typedef struct charon_element_inner charon_element_inner_t;

typedef struct charon_element {
    const charon_element_inner_t *inner;
    const struct charon_element_inner *parent;
    size_t offset;
} charon_element_t;

charon_element_cache_t *charon_element_cache_make(charon_memory_allocator_t *allocator);
void charon_element_cache_destroy(charon_element_cache_t *cache);

charon_element_inner_t *charon_element_inner_make_token(charon_element_cache_t *cache, charon_token_kind_t kind, const char *text);
charon_element_inner_t *charon_element_inner_make_node(charon_element_cache_t *cache, charon_node_kind_t kind, charon_element_inner_t *children[], size_t child_count);

charon_element_t *charon_element_root(charon_memory_allocator_t *allocator, charon_element_inner_t *inner_root);
