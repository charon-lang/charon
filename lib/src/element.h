#pragma once

#include "charon/element.h"

#include <stddef.h>
#include <stdint.h>

struct charon_element_inner {
    uint64_t hash;
    size_t length;

    charon_element_type_t type;
    union {
        struct {
            charon_trivia_kind_t kind;
            charon_utf8_text_t *text;
        } trivia;
        struct {
            charon_token_kind_t kind;
            charon_utf8_text_t *text;

            size_t leading_trivia_count, leading_trivia_length;
            size_t trailing_trivia_count, trailing_trivia_length;
            const struct charon_element_inner *trivia[];
        } token;
        struct {
            charon_node_kind_t kind;

            size_t child_count;
            const struct charon_element_inner *children[];
        } node;
    };
};
