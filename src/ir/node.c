#include "node.h"

#include <stdlib.h>

static ir_node_t *make_node(ir_node_type_t type, source_location_t source_location) {
    ir_node_t *node = malloc(sizeof(ir_node_t));
    node->type = type;
    node->source_location = source_location;
    node->next = NULL;
    return node;
}

ir_node_t *ir_node_make_root(ir_node_list_t tlc_nodes, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_ROOT, source_location);
    node->root.tlc_nodes = tlc_nodes;
    return node;
}

ir_node_t *ir_node_make_tlc_function(const char *name, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_TLC_FUNCTION, source_location);
    node->tlc_function.name = name;
    return node;
}

void ir_node_list_append(ir_node_list_t *list, ir_node_t *node) {
    if(list->first == NULL) list->first = node;
    node->next = list->last;
    list->last = node;
    list->count++;
}