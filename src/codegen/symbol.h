#pragma once

#include "codegen/codegen.h"
#include "ir/function.h"

#define SYMBOL_LIST_INIT (symbol_list_t) { .symbol_count = 0, .symbols = NULL }

typedef enum {
    SYMBOL_TYPE_FUNCTION,
    SYMBOL_TYPE_MODULE
} symbol_type_t;

typedef struct symbol_list {
    struct symbol *symbols;
    size_t symbol_count;
} symbol_list_t;

typedef struct symbol {
    const char *name;
    const char *full_name;
    symbol_type_t type;
    union {
        struct {
            ir_function_t *prototype;
            LLVMTypeRef type;
            LLVMValueRef value;
        } function;
        struct {
            struct symbol *parent;
            symbol_list_t symbols;
        } module;
    };
} symbol_t;

void symbol_list_free(symbol_list_t list);
symbol_t *symbol_list_find(symbol_list_t *list, const char *name);
symbol_t *symbol_list_find_type(symbol_list_t *list, const char *name, symbol_type_t type);
symbol_t *symbol_list_add_function(symbol_list_t *list, const char *name, const char *full_name, ir_function_t *prototype, LLVMTypeRef type, LLVMValueRef value);
symbol_t *symbol_list_add_module(symbol_list_t *list, const char *name, symbol_t *parent);