#pragma once

#include "charon/memory.h"
#include "charon/node.h"
#include "charon/token.h"
#include "charon/trivia.h"
#include "charon/utf8.h"

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
    struct charon_element *parent;
    size_t offset;
} charon_element_t;

charon_element_cache_t *charon_element_cache_make(charon_memory_allocator_t *allocator);
void charon_element_cache_destroy(charon_element_cache_t *cache);

/* Element makers */
const charon_element_inner_t *charon_element_inner_make_trivia(charon_element_cache_t *cache, charon_trivia_kind_t kind, charon_utf8_text_t *text);
const charon_element_inner_t *charon_element_inner_make_token(charon_element_cache_t *cache, charon_token_kind_t kind, charon_utf8_text_t *text, size_t leading_trivia_count, size_t trailing_trivia_count, const charon_element_inner_t *trivia[]);
const charon_element_inner_t *charon_element_inner_make_node(charon_element_cache_t *cache, charon_node_kind_t kind, const charon_element_inner_t *children[], size_t child_count);

/* Wrapper */
charon_element_t *charon_element_wrap_root(charon_memory_allocator_t *allocator, const charon_element_inner_t *inner_root);
charon_element_t *charon_element_wrap_node_child(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index);
charon_element_t *charon_element_wrap_token_leading_trivia(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index);
charon_element_t *charon_element_wrap_token_trailing_trivia(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index);

/* Generic Accessors */
charon_element_type_t charon_element_type(const charon_element_inner_t *inner_element);
size_t charon_element_length(const charon_element_inner_t *inner_element);

/* Trivia Accessors */
const charon_utf8_text_t *charon_element_trivia_text(const charon_element_inner_t *inner_element);
charon_trivia_kind_t charon_element_trivia_kind(const charon_element_inner_t *inner_element);

/* Token Accessors */
const charon_utf8_text_t *charon_element_token_text(const charon_element_inner_t *inner_element);
charon_token_kind_t charon_element_token_kind(const charon_element_inner_t *inner_element);
size_t charon_element_token_leading_trivia_count(const charon_element_inner_t *inner_element);
size_t charon_element_token_trailing_trivia_count(const charon_element_inner_t *inner_element);
size_t charon_element_token_leading_trivia_length(const charon_element_inner_t *inner_element);
size_t charon_element_token_trailing_trivia_length(const charon_element_inner_t *inner_element);
const charon_element_inner_t *charon_element_token_leading_trivia(const charon_element_inner_t *inner_element, size_t index);
const charon_element_inner_t *charon_element_token_trailing_trivia(const charon_element_inner_t *inner_element, size_t index);

/* Node Accessors */
charon_node_kind_t charon_element_node_kind(const charon_element_inner_t *inner_element);
size_t charon_element_node_child_count(const charon_element_inner_t *inner_element);
const charon_element_inner_t *charon_element_node_child(const charon_element_inner_t *inner_element, size_t index);
