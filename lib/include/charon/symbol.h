#pragma once

#include "charon/path.h"
#include "charon/utf8.h"

#include <stddef.h>

#define CHARON_SYMBOL_TABLE_INIT ((charon_symbol_table_t) { .symbols = nullptr, .symbol_count = 0 })

typedef enum {
    CHARON_SYMBOL_KIND_MODULE,
    CHARON_SYMBOL_KIND_FUNCTION,
    CHARON_SYMBOL_KIND_DECLARATION,
    CHARON_SYMBOL_KIND_ENUM,
} charon_symbol_kind_t;

typedef struct {
    struct charon_symbol **symbols;
    size_t symbol_count;
} charon_symbol_table_t;

// @todo: move this out of here
typedef struct {
    charon_utf8_text_t *name;
} charon_function_param_t;

typedef struct {
    charon_function_param_t **param_table;
    size_t param_count;
} charon_function_param_table_t;

typedef struct charon_symbol {
    charon_symbol_kind_t kind;
    charon_utf8_text_t *name;
    charon_path_t *definition;
    union {
        struct {
            charon_symbol_table_t table;
        } module;
        struct {
            charon_function_param_table_t* param_table;
            bool variadic;
        } function;
    };
} charon_symbol_t;
