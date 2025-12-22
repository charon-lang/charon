#include "charon/context.h"
#include "charon/element.h"
#include "charon/file.h"
#include "charon/interner.h"
#include "charon/memory.h"
#include "charon/node.h"
#include "charon/path.h"
#include "charon/utf8.h"
#include "platform.h"
#include "sema/context.h"
#include "sema/interner.h"
#include "sema/queries.h"
#include "sema/query.h"
#include "sema/symbol.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

void print_table(context_t *ctx, const symbol_table_t *table, int depth) {
    for(size_t i = 0; i < table->symbol_count; i++) {
        const symbol_t *symbol = context_lookup_symbol(ctx, table->symbols[i]);
        assert(symbol != nullptr);

        const charon_path_t *symbol_def_path = context_lookup_path(ctx, symbol->definition_path_id);
        assert(symbol_def_path != nullptr);

        const charon_file_t *file = context_lookup_file(ctx, symbol_def_path->file);
        assert(file != nullptr);

        const charon_element_inner_t *current = file->root_element;
        for(size_t j = 0; j < symbol_def_path->length; j++) {
            assert(charon_element_type(current) == CHARON_ELEMENT_TYPE_NODE);
            current = charon_element_node_child(current, symbol_def_path->indices[j]);
        }

        const charon_utf8_text_t *symbol_name = context_lookup_text(ctx, symbol->name_text_id);
        assert(symbol_name != nullptr);

        printf("%*s-> %s: \"%s\"", depth, "", charon_node_kind_tostring(charon_element_node_kind(current)), charon_utf8_as_string(symbol_name));
        if(symbol->kind == SYMBOL_KIND_MODULE) {
            const symbol_table_t *mod_table = context_lookup_symtab(ctx, symbol->module.symtab);
            printf(" | num_symbols: %zu\n", mod_table->symbol_count);
            print_table(ctx, mod_table, depth + 4);
        } else {
            printf("\n");
        }
    }
}

static void print_interner_state(interner_t *interner) {
    printf("\t%s: %lu\n", interner->label, interner->capacity - interner->free_count);
}

void test(charon_memory_allocator_t *allocator, const charon_file_t *file) {
    context_t *context = context_make();

    charon_interner_key_t file_key = interner_insert(&context->file_interner, (void *) file);

    query_engine_t *engine = query_engine_make(allocator, context);

    charon_interner_key_t symtab_key;
    assert(queries_symtab(engine, &file_key, &symtab_key) == QUERY_COMPUTE_STATUS_OK);

    {
        const symbol_table_t *symtab = context_lookup_symtab(context, symtab_key);
        assert(symtab != nullptr);

        printf("\nSymbol Table for %s:\n", file->name);
        print_table(context, symtab, 0);
    }

    printf("\nInterner States:\n");
    print_interner_state(&context->file_interner);
    print_interner_state(&context->text_interner);
    print_interner_state(&context->path_interner);
    print_interner_state(&context->symbol_interner);
    print_interner_state(&context->symtab_interner);

    interner_unref(&context->symtab_interner, symtab_key);

    printf("\nInterner States:\n");
    print_interner_state(&context->file_interner);
    print_interner_state(&context->text_interner);
    print_interner_state(&context->path_interner);
    print_interner_state(&context->symbol_interner);
    print_interner_state(&context->symtab_interner);

    query_engine_destroy(engine);
}
