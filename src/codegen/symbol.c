#include "symbol.h"

#include <string.h>
#include <stdlib.h>

symbol_t *symbol_table_find(symbol_table_t *symtab, const char *name) {
    for(size_t i = 0; i < symtab->symbol_count; i++) {
        if(strcmp(name, symtab->symbols[i].name) != 0) continue;
        return &symtab->symbols[i];
    }
    return NULL;
}

symbol_t *symbol_table_find_kind(symbol_table_t *symtab, const char *name, symbol_kind_t kind) {
    symbol_t *symbol = symbol_table_find(symtab, name);
    if(symbol == NULL || symbol->kind != kind) return NULL;
    return symbol;
}

static symbol_t *add_symbol(symbol_table_t *symtab, const char *name, symbol_kind_t kind) {
    symtab->symbols = reallocarray(symtab->symbols, ++symtab->symbol_count, sizeof(symbol_t));
    symbol_t *symbol = &symtab->symbols[symtab->symbol_count - 1];
    symbol->name = name;
    symbol->kind = kind;
    symbol->table = symtab;
    return symbol;
}

symbol_t *symbol_table_add_module(symbol_table_t *symtab, const char *name) {
    symbol_t *symbol = add_symbol(symtab, name, SYMBOL_KIND_MODULE);
    symbol->module.symtab = SYMBOL_TABLE_INIT;
    symbol->module.symtab.parent = symbol;
    return symbol;
}

symbol_t *symbol_table_add_function(symbol_table_t *symtab, const char *name, ir_type_t *type, LLVMValueRef llvm_value) {
    assert(type->kind == IR_TYPE_KIND_FUNCTION);
    symbol_t *symbol = add_symbol(symtab, name, SYMBOL_KIND_FUNCTION);
    symbol->function.type = type;
    symbol->function.llvm_value = llvm_value;
    return symbol;
}

bool symbol_table_exists(symbol_table_t *symtab, const char *name) {
    return symbol_table_find(symtab, name) != NULL;
}