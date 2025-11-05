#include "charon/element.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

const charon_element_inner_t *charon_util_element_swap_child(charon_element_cache_t *cache, const charon_element_inner_t *element, size_t index, const charon_element_inner_t *new_child) {
    assert(charon_element_type(element) == CHARON_ELEMENT_TYPE_NODE);

    size_t child_count = charon_element_node_child_count(element);
    const charon_element_inner_t **children = malloc(child_count * sizeof(charon_element_inner_t *));
    for(size_t i = 0; i < child_count; i++) children[i] = charon_element_node_child(element, i);
    children[index] = new_child;
    const charon_element_inner_t *new_element = charon_element_inner_make_node(cache, charon_element_node_kind(element), children, child_count);
    free(children);
    return new_element;
}

const charon_element_inner_t *charon_util_element_swap(charon_element_cache_t *cache, charon_element_t *old_subtree, const charon_element_inner_t *new_subtree) {
    charon_element_t *current = old_subtree;
    while(current->parent != nullptr) {
        charon_element_t *parent = current->parent;
        for(size_t i = 0; i < charon_element_node_child_count(parent->inner); i++) {
            const charon_element_inner_t *child = charon_element_node_child(parent->inner, i);
            if(child != current->inner) continue;

            new_subtree = charon_util_element_swap_child(cache, parent->inner, i, new_subtree);
            current = parent;
            break;
        }
    }

    return new_subtree;
}
