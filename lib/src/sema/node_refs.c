#include "charon/element.h"
#include "charon/file.h"
#include "charon/memory.h"
#include "charon/node.h"
#include "charon/path.h"
#include "charon/query.h"
#include "common//dynarray.h"
#include "context.h"
#include "node.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef DYNARRAY(size_t) indices_t;

static bool compute_node_ref_map(charon_query_engine_t *engine, charon_context_t *ctx, const void *key, void *value_out);

static void drop_node_ref_map(void *ptr);

charon_query_descriptor_t g_charon_queries_node_ref_map = {
    .name = "node_ref_map",
    .compute = compute_node_ref_map,
    .key_size = sizeof(size_t),
    .value_size = sizeof(node_ref_map_t),
    .value_drop = drop_node_ref_map,
};

static void drop_node_ref_map(void *ptr) {
    node_ref_map_t *map = ptr;
    for(size_t i = 0; i < map->element_count; i++) {
        charon_path_destroy(map->elements[i]->path);
        free(map->elements[i]);
    }
    free(map->elements);
    free(map);
}

static void node_ref_walk(charon_memory_allocator_t *allocator, charon_element_t *element, const charon_file_t *file, node_ref_map_t *node_ref_map, indices_t *indices) {
    switch(charon_element_type(element->inner)) {
        case CHARON_ELEMENT_TYPE_NODE:
            for(size_t i = 0; i < charon_element_node_child_count(element->inner); i++) {
                DYNARRAY_PUSH(indices, i);
                node_ref_walk(allocator, charon_element_wrap_node_child(allocator, element, i), file, node_ref_map, indices);
                DYNARRAY_POP(indices);
            }

            switch(charon_element_node_kind(element->inner)) {
                case CHARON_NODE_KIND_TLC_DECLARATION:     break;
                case CHARON_NODE_KIND_TLC_ENUMERATION:     break;
                case CHARON_NODE_KIND_TLC_EXTERN:          break;
                case CHARON_NODE_KIND_TLC_FUNCTION:        break;
                case CHARON_NODE_KIND_TLC_MODULE:          break;
                case CHARON_NODE_KIND_TLC_TYPE_DEFINITION: break;
                default:                                   return;
            }
            break;
        case CHARON_ELEMENT_TYPE_TOKEN:  return;
        case CHARON_ELEMENT_TYPE_TRIVIA: assert(false);
    }

    node_ref_t *ref = malloc(sizeof(node_ref_t));
    ref->file = file;
    ref->path = charon_path_make(indices->element_count);
    for(size_t i = 0; i < indices->element_count; i++) ref->path->steps[i] = indices->elements[i];

    DYNARRAY_PUSH(node_ref_map, ref);
}

static bool compute_node_ref_map(charon_query_engine_t *engine, charon_context_t *ctx, const void *key, void *value_out) {
    const charon_file_t *file = context_lookup_file(ctx, *(size_t *) key);

    node_ref_map_t node_ref_map = DYNARRAY_INIT;

    charon_memory_allocator_t *allocator = charon_memory_allocator_make();
    node_ref_walk(allocator, charon_element_wrap_root(allocator, file->root_element), file, &node_ref_map, &(indices_t) DYNARRAY_INIT);
    charon_memory_allocator_free(allocator);

    *(node_ref_map_t *) value_out = node_ref_map;

    return true;
}
