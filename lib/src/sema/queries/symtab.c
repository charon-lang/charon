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
#include "platform.h"
#include "sema/context.h"
#include "sema/queries.h"
#include "sema/query.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef DYNARRAY(size_t) indices_t;

static bool compute_symtab(query_engine_t *engine, context_t *ctx, const void *key, void *value_out);

const query_descriptor_t g_queries_symtab_descriptor = {
    .name = "symtab",
    .compute = compute_symtab,
    .key_size = sizeof(size_t),
    .value_size = sizeof(charon_symbol_table_t),
};

static bool insert_symbol(charon_symbol_table_t *table, charon_symbol_t *symbol) {
    table->symbols = reallocarray(table->symbols, table->symbol_count + 1, sizeof(charon_symbol_t));
    if(table->symbols == nullptr) {
        printf("[insert_symbol] Failed to allocate memory for symbol table\n");
        return false;
    }

    table->symbols[table->symbol_count] = symbol;
    table->symbol_count += 1;

    return true;
}

static bool parse_module(charon_memory_allocator_t *allocator, charon_element_t *element, charon_symbol_table_t **table, charon_path_t *path) {
    charon_symbol_t *module_symbol = malloc(sizeof(charon_symbol_t));
    module_symbol->definition = path;
    module_symbol->kind = CHARON_SYMBOL_KIND_MODULE;
    module_symbol->module.table = CHARON_SYMBOL_TABLE_INIT;

    const charon_element_inner_t *name_element = charon_element_node_child(element->inner, 1);
    if(charon_element_type(name_element) != CHARON_ELEMENT_TYPE_TOKEN || charon_element_token_kind(name_element) != CHARON_TOKEN_KIND_IDENTIFIER) {
        // TODO: throw error diagnostics here!!!!
        printf("[symtab_walk] Expected identifier token for module name\n");
        return false;
    }

    module_symbol->name = utf8_copy(charon_element_token_text(name_element));
    if(!insert_symbol(*table, module_symbol)) {
        return false;
    }
    *table = &module_symbol->module.table;
    return true;
}

static bool parse_function(charon_memory_allocator_t *allocator, charon_element_t *element, charon_symbol_table_t *table, charon_path_t *path) {
    charon_symbol_t *function_symbol = malloc(sizeof(charon_symbol_t));
    function_symbol->definition = path;
    function_symbol->kind = CHARON_SYMBOL_KIND_FUNCTION;
    const charon_element_inner_t *func_name_element = charon_element_node_child(element->inner, 1);
    if(charon_element_type(func_name_element) != CHARON_ELEMENT_TYPE_TOKEN || charon_element_token_kind(func_name_element) != CHARON_TOKEN_KIND_IDENTIFIER) {
        // TODO: throw error diagnostics here!!!!
        printf("[symtab_walk] Expected identifier token for function name\n");
        return false;
    }

    function_symbol->name = utf8_copy(charon_element_token_text(func_name_element));
    function_symbol->function.param_table = malloc(sizeof(charon_function_param_table_t));
    function_symbol->function.param_table->param_count = 0;
    function_symbol->function.param_table->param_table = nullptr;

    const charon_element_inner_t *params_element = charon_element_node_child(element->inner, 2);
    const charon_element_inner_t *first_param_token = charon_element_node_child(params_element, 0);
    // @todo: this code is fucking ugly as fuck, and breaks if it's a node and not a token ig
    if(charon_element_token_kind(first_param_token) != CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT) {
        // TODO: throw error diagnostics here!!!!
        printf("dying on token kind: %s\n", charon_token_kind_tostring(charon_element_token_kind(charon_element_node_child(params_element, 0))));
        return false;
    }

    int index = 1;
    // @todo: THIS FUCKING SUCKS
    while(true) {
        const charon_element_inner_t *preprocessing_element = charon_element_node_child(params_element, index);
        if(charon_element_type(preprocessing_element) == CHARON_ELEMENT_TYPE_TOKEN && charon_element_token_kind(preprocessing_element) == CHARON_TOKEN_KIND_PNCT_COMMA) {
            index++;
        }
        if(charon_element_type(preprocessing_element) == CHARON_ELEMENT_TYPE_TOKEN && charon_element_token_kind(preprocessing_element) == CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT) {
            break;
        }
        const charon_element_inner_t *current_element = charon_element_node_child(params_element, index);

        // @todo: this won't work if they don't put the var args at the end we should prob check for that because otherwise it's invalid
        if(charon_element_type(current_element) == CHARON_ELEMENT_TYPE_TOKEN && charon_element_token_kind(current_element) == CHARON_TOKEN_KIND_PNCT_TRIPLE_PERIOD) {
            function_symbol->function.variadic = true;
            break;
        }

        printf("found param kind: %d @ %d\n", charon_element_type(current_element), index);
        if(charon_element_type(current_element) == CHARON_ELEMENT_TYPE_TOKEN && charon_element_token_kind(current_element) == CHARON_TOKEN_KIND_IDENTIFIER) {
            printf("param name: %s\n", charon_utf8_as_string(charon_element_token_text(current_element)));
        } else {
            printf("unexpected param element kind %s\n", charon_token_kind_tostring(charon_element_token_kind(current_element)));
        }

        // @todo: refactor
        {
            charon_function_param_table_t *pt = function_symbol->function.param_table;
            /* grow the inner array of param pointers (charon_function_param_t*) */
            charon_function_param_t **new_table = reallocarray(pt->param_table, pt->param_count + 1, sizeof(charon_function_param_t *));
            if(new_table == nullptr) {
                printf("[parse_function] Failed to allocate memory for param table\n");
                return false;
            }
            pt->param_table = new_table;
        }

        charon_function_param_t *param = malloc(sizeof(charon_function_param_t));
        if(param == NULL) {
            printf("[parse_function] Failed to allocate memory for param\n");
            return false;
        }
        param->name = utf8_copy(charon_element_token_text(current_element));
        function_symbol->function.param_table->param_table[function_symbol->function.param_table->param_count] = param;
        function_symbol->function.param_table->param_count += 1;
        // @todo: we skip the ident, colon, and type for now
        index += 3;
    }

    printf("params_element child count: %zu\n", charon_element_node_child_count(params_element));
    printf("params_element kind: %s\n", charon_node_kind_tostring(charon_element_node_kind(params_element)));
    printf("function '%s' has %zu params\n", charon_utf8_as_string(function_symbol->name), function_symbol->function.param_table->param_count);
    printf("\n");
    return insert_symbol(table, function_symbol);
}

static bool symtab_walk(charon_memory_allocator_t *allocator, charon_element_t *element, const charon_file_t *file, charon_symbol_table_t *table, indices_t *indices) {
    switch(charon_element_type(element->inner)) {
        case CHARON_ELEMENT_TYPE_NODE:   break;
        case CHARON_ELEMENT_TYPE_TOKEN:  return true;
        case CHARON_ELEMENT_TYPE_TRIVIA: assert(false);
    }

    charon_path_t *path = charon_path_make(indices->element_count);
    path->file_id = file->file_id;
    for(size_t i = 0; i < indices->element_count; i++) path->indices[i] = indices->elements[i];

    bool success = false;
    switch(charon_element_node_kind(element->inner)) {
        case CHARON_NODE_KIND_TLC_MODULE:   success = parse_module(allocator, element, &table, path); break;
        case CHARON_NODE_KIND_TLC_FUNCTION: success = parse_function(allocator, element, table, path); break;
        default:                            success = true; break;
    }

    // @note: is this correct?
    if(!success) {
        charon_path_destroy(path);
        return false;
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
        printf("[compute_symtab] Failed to compute symbol table\n");
        return false;
    }

    *(charon_symbol_table_t *) value_out = symbol_table;

    return true;
}
