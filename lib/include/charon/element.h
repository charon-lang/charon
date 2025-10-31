#pragma once

#include "charon/memory.h"
#include "charon/node.h"
#include "charon/token.h"
#include "charon/trivia.h"

#include <stddef.h>

typedef enum {
    CHARON_ELEMENT_TYPE_TRIVIA,
    CHARON_ELEMENT_TYPE_TOKEN,
    CHARON_ELEMENT_TYPE_NODE
} charon_element_type_t;

typedef struct charon_element_cache charon_element_cache_t;
typedef struct charon_element_inner charon_element_inner_t;

typedef struct charon_element {
    const charon_element_inner_t *inner;
    const struct charon_element *parent;
    size_t offset;
} charon_element_t;

charon_element_cache_t *charon_element_cache_make(charon_memory_allocator_t *allocator);
void charon_element_cache_destroy(charon_element_cache_t *cache);

/* Element makers */
charon_element_inner_t *charon_element_inner_make_trivia(charon_element_cache_t *cache, charon_trivia_kind_t kind, const char *text);
charon_element_inner_t *charon_element_inner_make_token(charon_element_cache_t *cache, charon_token_kind_t kind, const char *text, size_t leading_trivia_count, size_t trailing_trivia_count, charon_element_inner_t *trivia[]);
charon_element_inner_t *charon_element_inner_make_node(charon_element_cache_t *cache, charon_node_kind_t kind, charon_element_inner_t *children[], size_t child_count);

/* Wrapper */
charon_element_t *charon_element_root(charon_memory_allocator_t *allocator, charon_element_inner_t *inner_root);

/* Generic Accessors */
charon_element_type_t charon_element_type(charon_element_t *element);
size_t charon_element_offset(charon_element_t *element);
size_t charon_element_length(charon_element_t *element);

/* Trivia Accessors */
const char *charon_element_inner_trivia_text(const charon_element_inner_t *inner_element);
charon_trivia_kind_t charon_element_inner_trivia_kind(const charon_element_inner_t *inner_element);

const char *charon_element_trivia_text(charon_element_t *element);
charon_trivia_kind_t charon_element_trivia_kind(charon_element_t *element);

/* Token Accessors */
const char *charon_element_inner_token_text(const charon_element_inner_t *inner_element);
charon_token_kind_t charon_element_inner_token_kind(const charon_element_inner_t *inner_element);
size_t charon_element_inner_token_leading_trivia_count(const charon_element_inner_t *inner_element);
size_t charon_element_inner_token_trailing_trivia_count(const charon_element_inner_t *inner_element);
size_t charon_element_inner_token_leading_trivia_length(const charon_element_inner_t *inner_element);
size_t charon_element_inner_token_trailing_trivia_length(const charon_element_inner_t *inner_element);

const char *charon_element_token_text(charon_element_t *element);
charon_token_kind_t charon_element_token_kind(charon_element_t *element);
size_t charon_element_token_leading_trivia_count(charon_element_t *element);
size_t charon_element_token_trailing_trivia_count(charon_element_t *element);
size_t charon_element_token_leading_trivia_length(charon_element_t *element);
size_t charon_element_token_trailing_trivia_length(charon_element_t *element);
charon_element_t *charon_element_token_leading_trivia(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index);
charon_element_t *charon_element_token_trailing_trivia(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index);

/* Node Accessors */
charon_node_kind_t charon_element_inner_node_kind(const charon_element_inner_t *inner_element);
size_t charon_element_inner_node_child_count(const charon_element_inner_t *inner_element);

charon_node_kind_t charon_element_node_kind(charon_element_t *element);
size_t charon_element_node_child_count(charon_element_t *element);
charon_element_t *charon_element_node_child(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index);
