#include "ast/function.h"
#include "ast/module.h"
#include "charon/context.h"
#include "charon/element.h"
#include "charon/file.h"
#include "charon/interner.h"
#include "charon/memory.h"
#include "charon/node.h"
#include "charon/path.h"
#include "charon/utf8.h"
#include "common/dynarray.h"
#include "platform.h"
#include "sema/context.h"
#include "sema/interner.h"
#include "sema/queries.h"
#include "sema/query.h"
#include "sema/symbol.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef DYNARRAY(size_t) indices_t;

static query_compute_status_t compute_symtab(query_engine_t *engine, context_t *ctx, const void *key, void *value_out);

static void value_drop(context_t *ctx, void *value) {
    interner_unref(&ctx->symtab_interner, *(charon_interner_key_t *) value);
}

const query_descriptor_t g_queries_symtab_descriptor = {
    .name = "symtab",
    .compute = compute_symtab,
    .key_size = sizeof(size_t),
    .value_size = sizeof(charon_interner_key_t),
    .value_drop = value_drop,
};

static symbol_table_t *insert_symbol(symbol_table_t *table, charon_interner_key_t symbol_key) {
    table = realloc(table, sizeof(symbol_table_t) + (++table->symbol_count * sizeof(charon_interner_key_t)));
    table->symbols[table->symbol_count - 1] = symbol_key;
    return table;
}

static bool symtab_walk(context_t *ctx, charon_memory_allocator_t *allocator, charon_element_t *element, charon_interner_key_t file, symbol_table_t **table, indices_t *indices) {
    switch(charon_element_type(element->inner)) {
        case CHARON_ELEMENT_TYPE_NODE:   break;
        case CHARON_ELEMENT_TYPE_TOKEN:  return true;
        case CHARON_ELEMENT_TYPE_TRIVIA: assert(false);
    }

    symbol_table_t *new_table = nullptr;
    symbol_t *symbol = nullptr;
    switch(charon_element_node_kind(element->inner)) {
        case CHARON_NODE_KIND_TLC_MODULE: {
            symbol = malloc(sizeof(symbol_t));

            charon_utf8_text_t *name = ast_node_module_name((ast_node_module_t) { .inner_element = element->inner });
            if(name == nullptr) {
                free(symbol);
                return false;
            }

            symbol->kind = SYMBOL_KIND_MODULE;
            symbol->name_text_id = context_intern_text(ctx, name);

            *table = insert_symbol(*table, context_intern_symbol(ctx, symbol));

            new_table = malloc(sizeof(symbol_table_t));
            new_table->symbol_count = 0;
            table = &new_table;

            goto set_path;
        }

        case CHARON_NODE_KIND_TLC_FUNCTION: {
            symbol = malloc(sizeof(symbol_t));

            charon_utf8_text_t *name = ast_node_function_name((ast_node_function_t) { .inner_element = element->inner });
            if(name == nullptr) {
                free(symbol);
                return false;
            }

            symbol->kind = SYMBOL_KIND_FUNCTION;
            symbol->name_text_id = context_intern_text(ctx, name);

            *table = insert_symbol(*table, context_intern_symbol(ctx, symbol));

            goto set_path;
        }

        set_path: {
            charon_path_t *path = charon_path_make(indices->element_count);
            interner_ref(&ctx->file_interner, file);
            path->file = file;
            for(size_t i = 0; i < indices->element_count; i++) path->indices[i] = indices->elements[i];

            symbol->definition_path_id = context_intern_path(ctx, path);
            break;
        }

        default: break;
    }

    for(size_t i = 0; i < charon_element_node_child_count(element->inner); i++) {
        DYNARRAY_PUSH(indices, i);
        symtab_walk(ctx, allocator, charon_element_wrap_node_child(allocator, element, i), file, table, indices);
        DYNARRAY_POP(indices);
    }

    if(charon_element_node_kind(element->inner) == CHARON_NODE_KIND_TLC_MODULE) {
        symbol->module.symtab = context_intern_symtab(ctx, new_table);
    }

    return true;
}

static query_compute_status_t compute_symtab(query_engine_t *engine, context_t *ctx, const void *key, void *value_out) {
    charon_interner_key_t interner_key = *(charon_interner_key_t *) key;

    const charon_file_t *file = context_lookup_file(ctx, interner_key);
    assert(file != nullptr);

    symbol_table_t *symbol_table = malloc(sizeof(symbol_table_t));
    symbol_table->symbol_count = 0;

    charon_memory_allocator_t *allocator = charon_memory_allocator_make();
    bool symtab_success = symtab_walk(ctx, allocator, charon_element_wrap_root(allocator, file->root_element), interner_key, &symbol_table, &(indices_t) DYNARRAY_INIT);

    interner_unref(&ctx->file_interner, interner_key);

    *(charon_interner_key_t *) value_out = context_intern_symtab(ctx, symbol_table);

    return symtab_success ? QUERY_COMPUTE_STATUS_OK : QUERY_COMPUTE_STATUS_FAIL;
}
