#pragma once

#include "charon/syntax/node.h"
#include "charon/syntax/token.h"
#include "charon/syntax/trivia.h"
#include "core/db.h"
#include "core/store.h"

#include <stddef.h>
#include <stdint.h>

typedef enum {
    ELEMENT_TYPE_TRIVIA,
    ELEMENT_TYPE_TOKEN,
    ELEMENT_TYPE_NODE,
} element_type_t;

typedef struct element_inner {
    size_t length;
    element_type_t type;
    union {
        struct {
            charon_trivia_kind_t kind;
            store_handle_t text;
        } trivia;
        struct {
            charon_token_kind_t kind;
            store_handle_t text;

            size_t leading_trivia_count, leading_trivia_length;
            size_t trailing_trivia_count, trailing_trivia_length;
            store_handle_t trivia[];
        } token;
        struct {
            charon_node_kind_t kind;

            size_t child_count;
            store_handle_t children[];
        } node;
    };
} element_inner_t;

typedef struct charon_element {
    const element_inner_t *inner;
    struct charon_element *parent;
    size_t offset;
    size_t self_index;
} charon_element_t;

size_t element_inner_size(const element_inner_t *inner_element);
bool element_inner_equal(const element_inner_t *a, const element_inner_t *b);
uint64_t element_inner_hash(store_t *store, const element_inner_t *inner_element);

store_handle_t element_inner_make_trivia(db_t *db, charon_trivia_kind_t kind, store_handle_t text);
store_handle_t element_inner_make_token(db_t *db, charon_token_kind_t kind, store_handle_t text, size_t leading_trivia_count, size_t trailing_trivia_count, store_handle_t trivia[]);
store_handle_t element_inner_make_node(db_t *db, charon_node_kind_t kind, size_t child_count, store_handle_t children[]);
