#include "charon/context.h"
#include "charon/element.h"
#include "charon/file.h"
#include "charon/memory.h"
#include "charon/node.h"
#include "charon/queries.h"
#include "charon/query.h"
#include "common//dynarray.h"
#include "context.h"
#include "node.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

void test(charon_memory_allocator_t *allocator, const charon_file_t *file) {
    charon_context_t *context = context_make();

    context_file_entry_t entry = { .file_id = context->file_id_counter++, .file = file };
    DYNARRAY_PUSH(&context->file_entries, entry);


    const node_ref_map_t *ref_map;
    charon_query_engine_t *engine = charon_query_engine_make(allocator, context);
    charon_query_engine_execute(engine, &g_charon_queries_node_ref_map, &entry.file_id, (const void **) &ref_map);


    printf("map size %lu\n", ref_map->element_count);
    for(size_t i = 0; i < ref_map->element_count; i++) {
        const node_ref_t *node_ref = ref_map->elements[i];

        const charon_element_inner_t *current = file->root_element;
        for(size_t j = 0; j < node_ref->path->length; j++) {
            assert(charon_element_type(current) == CHARON_ELEMENT_TYPE_NODE);
            current = charon_element_node_child(current, node_ref->path->steps[j]);
        }

        printf("-> %s [%s]\n", node_ref->file->name, charon_node_kind_tostring(charon_element_node_kind(current)));
    }

    charon_query_engine_destroy(engine);
}
