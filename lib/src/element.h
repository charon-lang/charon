#pragma once

#include "charon/element.h"

#include <stdint.h>

typedef enum {
    CHARON_ELEMENT_TYPE_TOKEN,
    CHARON_ELEMENT_TYPE_NODE
} element_type_t;

struct charon_element_inner {
    size_t length;
    uint64_t hash;

    element_type_t type;
    union {
        struct {
            charon_token_kind_t kind;
            const char *text;
        } token;
        struct {
            charon_node_kind_t kind;
            size_t child_count;
            const struct charon_element_inner *children[];
        } node;
    };
};
