#pragma once

#include "charon/interner.h"

#include <stddef.h>

typedef enum {
    SYMBOL_KIND_MODULE,
    SYMBOL_KIND_FUNCTION,
    SYMBOL_KIND_DECLARATION,
    SYMBOL_KIND_ENUM,
} symbol_kind_t;

typedef struct {
    size_t symbol_count;
    charon_interner_key_t symbols[];
} symbol_table_t;

typedef struct symbol {
    symbol_kind_t kind;
    charon_interner_key_t name_text_id;
    charon_interner_key_t definition_path_id;
    union {
        struct {
            charon_interner_key_t symtab;
        } module;
    };
} symbol_t;
