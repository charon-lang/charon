#pragma once

#include "charon/node.h"
#include "charon/token.h"

#include <stddef.h>
#include <stdint.h>

typedef struct charon_element_cache charon_element_cache_t;

typedef enum charon_element_type {
    CHARON_ELEMENT_TYPE_TOKEN,
    CHARON_ELEMENT_TYPE_NODE
} charon_element_type_t;

typedef struct charon_element {
    size_t length;
    uint64_t hash;

    charon_element_type_t type;
    union {
        charon_token_t token;
        struct {
            charon_node_kind_t kind;
            size_t child_count;
            const struct charon_element *children[];
        } node;
    };
} charon_element_t;

charon_element_cache_t *charon_element_cache_make();
void charon_element_cache_destroy(charon_element_cache_t *cache);

charon_element_t *charon_element_make_token(charon_element_cache_t *cache, charon_token_kind_t kind, const char *text);
charon_element_t *charon_element_make_node(charon_element_cache_t *cache, charon_node_kind_t kind, charon_element_t *children[], size_t child_count);
