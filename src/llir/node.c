#include "node.h"

size_t llir_node_list_count(llir_node_list_t *list) {
    return list->count;
}

void llir_node_list_append(llir_node_list_t *list, llir_node_t *node) {
    if(list->first == NULL) list->first = node;
    if(list->last != NULL) list->last->next = node;
    list->last = node;
    list->count++;
}