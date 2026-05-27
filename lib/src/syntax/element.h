#pragma once

#include "charon/syntax/node.h"
#include "charon/syntax/token.h"
#include "charon/syntax/trivia.h"
#include "common/text.h"
#include "core/db.h"

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

            const text_t *text;
        } trivia;
        struct {
            charon_token_kind_t kind;

            const text_t *text;

            size_t leading_trivia_count, leading_trivia_length;
            size_t trailing_trivia_count, trailing_trivia_length;
            const struct element_inner *trivia[];
        } token;
        struct {
            charon_node_kind_t kind;

            size_t child_count;
            const struct element_inner *children[];
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
uint64_t element_inner_hash(const element_inner_t *inner_element);
bool element_inner_equal(const element_inner_t *a, const element_inner_t *b);

const element_inner_t *element_inner_make_trivia(db_t *db, charon_trivia_kind_t kind, const text_t *text);
const element_inner_t *element_inner_make_token(db_t *db, charon_token_kind_t kind, const text_t *text, size_t leading_trivia_count, size_t trailing_trivia_count, const element_inner_t *trivia[]);
const element_inner_t *element_inner_make_node(db_t *db, charon_node_kind_t kind, size_t child_count, const element_inner_t *children[]);
