#include "charon/context.h"
#include "charon/element.h"
#include "charon/file.h"
#include "charon/memory.h"
#include "charon/node.h"
#include "charon/symbol.h"
#include "charon/utf8.h"
#include "common//dynarray.h"
#include "platform.h"
#include "sema/context.h"
#include "sema/queries.h"
#include "sema/query.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

void print_table(context_t *ctx, const charon_symbol_table_t *table, int depth) {
    for(size_t i = 0; i < table->symbol_count; i++) {
        const charon_symbol_t *symbol = table->symbols[i];

        const charon_file_t *file = context_lookup_file(ctx, symbol->definition->file_id);
        assert(file != nullptr);

        const charon_element_inner_t *current = file->root_element;
        for(size_t j = 0; j < symbol->definition->length; j++) {
            assert(charon_element_type(current) == CHARON_ELEMENT_TYPE_NODE);
            current = charon_element_node_child(current, symbol->definition->indices[j]);
        }

        printf("%*s-> %s: \"%s\"", depth, "", charon_node_kind_tostring(charon_element_node_kind(current)), charon_utf8_as_string(symbol->name));
        if(symbol->kind == CHARON_SYMBOL_KIND_MODULE) {
            printf(" | num_symbols: %zu\n", symbol->module.table.symbol_count);
            print_table(ctx, &symbol->module.table, depth + 4);
        } else if(symbol->kind == CHARON_SYMBOL_KIND_FUNCTION) {
            printf(" | num_params: %zu\n", symbol->function.param_table->param_count);
            for(size_t p = 0; p < symbol->function.param_table->param_count; p++) {
                charon_function_param_t *param = symbol->function.param_table->param_table[p];
                printf("%*s   - Param %zu: \"%s\"\n", depth + 4, "", p, charon_utf8_as_string(param->name));
            }
        }
    }
}

void test(charon_memory_allocator_t *allocator, const charon_file_t *file) {
    context_t *context = context_make();

    context_file_entry_t entry = { .file_id = context->file_id_counter++, .file = file };
    DYNARRAY_PUSH(&context->file_entries, entry);

    query_engine_t *engine = query_engine_make(allocator, context);

    const charon_symbol_table_t *symtab;
    assert(queries_symtab(engine, &entry.file_id, &symtab));
    printf("\nSymbol Table for %s:\n", file->name);
    print_table(context, symtab, 0);

    query_engine_destroy(engine);
}
