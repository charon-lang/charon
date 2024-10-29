#pragma once

#include "codegen/codegen.h"
#include "ir/type.h"

#define SYMBOL_TABLE_INIT (symbol_table_t) { .parent = NULL, .symbols = NULL, .symbol_count = 0 }

typedef enum {
    SYMBOL_KIND_MODULE,
    SYMBOL_KIND_FUNCTION,
    SYMBOL_KIND_VARIABLE
} symbol_kind_t;

typedef struct symbol symbol_t;
typedef struct symbol_table symbol_table_t;

struct symbol_table {
    symbol_t *parent;
    symbol_t *symbols;
    size_t symbol_count;
};

struct symbol {
    const char *name;
    symbol_kind_t kind;
    symbol_table_t *table;
    union {
        struct {
            symbol_table_t symtab;
        } module;
        struct {
            ir_type_t *type;
            LLVMValueRef llvm_value;
        } function;
    };
};

symbol_t *symbol_table_find(symbol_table_t *symtab, const char *name);
symbol_t *symbol_table_find_kind(symbol_table_t *symtab, const char *name, symbol_kind_t kind);
symbol_t *symbol_table_add_module(symbol_table_t *symtab, const char *name);
symbol_t *symbol_table_add_function(symbol_table_t *symtab, const char *name, ir_type_t *type, LLVMValueRef llvm_value);
bool symbol_table_exists(symbol_table_t *symtab, const char *name);