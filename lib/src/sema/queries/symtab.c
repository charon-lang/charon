#include "charon/context.h"
#include "charon/element.h"
#include "charon/file.h"
#include "charon/memory.h"
#include "charon/node.h"
#include "charon/path.h"
#include "charon/symbol.h"
#include "charon/token.h"
#include "common/dynarray.h"
#include "common/utf8.h"
#include "sema/context.h"
#include "sema/queries.h"
#include "sema/query.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

typedef DYNARRAY(size_t) indices_t;

static bool compute_symtab(query_engine_t *engine, context_t *ctx, const void *key, void *value_out);

const query_descriptor_t g_queries_symtab_descriptor = {
    .name = "symtab",
    .compute = compute_symtab,
    .key_size = sizeof(size_t),
    .value_size = sizeof(charon_symbol_table_t),
};

static bool symtab_walk(charon_memory_allocator_t *allocator, charon_element_t *element, const charon_file_t *file, charon_symbol_table_t *table, indices_t *indices) {
    switch(charon_element_type(element->inner)) {
        case CHARON_ELEMENT_TYPE_NODE:   break;
        case CHARON_ELEMENT_TYPE_TOKEN:  return true;
        case CHARON_ELEMENT_TYPE_TRIVIA: assert(false);
    }

    charon_path_t *path = charon_path_make(indices->element_count);
    path->file_id = file->file_id;
    for(size_t i = 0; i < indices->element_count; i++) path->indices[i] = indices->elements[i];

    switch(charon_element_node_kind(element->inner)) {
        case CHARON_NODE_KIND_TLC_MODULE:
            charon_symbol_t *module_symbol = malloc(sizeof(charon_symbol_t));
            module_symbol->definition = path;
            module_symbol->kind = CHARON_SYMBOL_KIND_MODULE;
            module_symbol->module.table = CHARON_SYMBOL_TABLE_INIT;

            const charon_element_inner_t *name_element = charon_element_node_child(element->inner, 1);
            if(charon_element_type(name_element) != CHARON_ELEMENT_TYPE_TOKEN || charon_element_token_kind(name_element) != CHARON_TOKEN_KIND_IDENTIFIER) {
                // TODO: throw error diagnostics here!!!!
                return false;
            }

            module_symbol->name = utf8_copy(charon_element_token_text(name_element));

            table->symbols = reallocarray(table->symbols, ++table->symbol_count, sizeof(charon_symbol_t));
            table->symbols[table->symbol_count - 1] = module_symbol;

            table = &module_symbol->module.table;
            break;
        default: break;
    }

    for(size_t i = 0; i < charon_element_node_child_count(element->inner); i++) {
        DYNARRAY_PUSH(indices, i);
        symtab_walk(allocator, charon_element_wrap_node_child(allocator, element, i), file, table, indices);
        DYNARRAY_POP(indices);
    }

    return true;
}

static bool compute_symtab(query_engine_t *engine, context_t *ctx, const void *key, void *value_out) {
    const charon_file_t *file = context_lookup_file(ctx, *(size_t *) key);
    if(file == nullptr) return false;

    charon_symbol_table_t symbol_table = { .symbol_count = 0, .symbols = nullptr };

    charon_memory_allocator_t *allocator = charon_memory_allocator_make();
    if(!symtab_walk(allocator, charon_element_wrap_root(allocator, file->root_element), file, &symbol_table, &(indices_t) DYNARRAY_INIT)) {
        // TODO: cleanup whatever fucking shi the fucking walk created
        return false;
    }

    *(charon_symbol_table_t *) value_out = symbol_table;

    return true;
}
