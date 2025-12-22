#include "context.h"

#include "charon/context.h"
#include "charon/path.h"
#include "sema/interner.h"
#include "sema/symbol.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

static void cleanup_path(context_t *ctx, charon_path_t *path) {
    interner_unref(&ctx->file_interner, path->file);
    free(path);
}

static void cleanup_symbol(context_t *ctx, symbol_t *symbol) {
    interner_unref(&ctx->text_interner, symbol->name_text_id);
    interner_unref(&ctx->path_interner, symbol->definition_path_id);
    switch(symbol->kind) {
        case SYMBOL_KIND_MODULE: interner_unref(&ctx->symtab_interner, symbol->module.symtab); break;
        default:                 break;
    }
    free(symbol);
}

static void cleanup_symtab(context_t *ctx, symbol_table_t *symtab) {
    for(size_t i = 0; i < symtab->symbol_count; i++) {
        interner_unref(&ctx->symbol_interner, symtab->symbols[i]);
    }
    free(symtab);
}

context_t *context_make() {
    context_t *ctx = malloc(sizeof(context_t));
    ctx->file_interner = INTERNER_INIT("file", nullptr, nullptr);
    ctx->text_interner = INTERNER_INIT("text", nullptr, nullptr);
    ctx->path_interner = INTERNER_INIT("path", (void (*)(void *, void *)) cleanup_path, ctx);
    ctx->symbol_interner = INTERNER_INIT("symbol", (void (*)(void *, void *)) cleanup_symbol, ctx);
    ctx->symtab_interner = INTERNER_INIT("symtab", (void (*)(void *, void *)) cleanup_symtab, ctx);
    return ctx;
}

void context_destroy(context_t *ctx) {
    free(ctx);
}
