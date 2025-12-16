#pragma once

#include "charon/element.h"

const charon_element_inner_t *charon_util_element_swap_child(charon_element_cache_t *cache, const charon_element_inner_t *element, size_t index, const charon_element_inner_t *new_child);
const charon_element_inner_t *charon_util_element_swap(charon_element_cache_t *cache, charon_element_t *old_subtree, const charon_element_inner_t *new_subtree);
void charon_util_element_walk_tree(charon_memory_allocator_t *allocator, void (*fn)(charon_element_t *element, void *payload), void *payload, charon_element_t *element);
