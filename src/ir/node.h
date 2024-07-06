#pragma once

#include "lib/source.h"

#define IR_NODE_LIST_INIT ((ir_node_list_t) { .count = 0, .first = NULL, .last = NULL })

typedef enum {
    IR_NODE_TYPE_ROOT,

    IR_NODE_TYPE_TLC_FUNCTION
} ir_node_type_t;

typedef struct ir_node ir_node_t;

typedef struct {
    size_t count;
    ir_node_t *first, *last;
} ir_node_list_t;

struct ir_node {
    ir_node_type_t type;
    source_location_t source_location;
    union {
        struct {
            ir_node_list_t tlc_nodes;
        } root;

        struct {
            const char *name;
        } tlc_function;
    };
    struct ir_node *next;
};

ir_node_t *ir_node_make_root(ir_node_list_t tlc_nodes, source_location_t source_location);

ir_node_t *ir_node_make_tlc_function(const char *name, source_location_t source_location);

void ir_node_list_append(ir_node_list_t *list, ir_node_t *node);