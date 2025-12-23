#pragma once

#include "charon/context.h"
#include "charon/file.h"
#include "charon/path.h"
#include "charon/utf8.h"
#include "core/interner.h"
#include "sema/symbol.h"

#include <stddef.h>

#define CONTEXT_DECLARE_INTERNER_FNS(NAME, TYPE)                                                        \
    static inline const TYPE *context_lookup_##NAME(context_t *ctx, charon_interner_key_t NAME##_key) { \
        return interner_lookup(&ctx->NAME##_interner, NAME##_key);                                      \
    }                                                                                                   \
    static inline charon_interner_key_t context_intern_##NAME(context_t *ctx, TYPE *NAME) {             \
        return interner_insert(&ctx->NAME##_interner, (void *) NAME);                                   \
    }

struct charon_context {
    interner_t file_interner;
    interner_t text_interner;
    interner_t path_interner;
    interner_t symbol_interner;
    interner_t symtab_interner;
};

context_t *context_make();
void context_destroy(context_t *ctx);

CONTEXT_DECLARE_INTERNER_FNS(file, charon_file_t)
CONTEXT_DECLARE_INTERNER_FNS(text, charon_utf8_text_t)
CONTEXT_DECLARE_INTERNER_FNS(path, charon_path_t)
CONTEXT_DECLARE_INTERNER_FNS(symbol, symbol_t)
CONTEXT_DECLARE_INTERNER_FNS(symtab, symbol_table_t)
