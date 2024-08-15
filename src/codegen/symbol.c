#include "symbol.h"

#include <stdlib.h>
#include <string.h>

static symbol_t *add_symbol(symbol_list_t *list, const char *name, const char *full_name, symbol_type_t symbol_type) {
    list->symbols = reallocarray(list->symbols, ++list->symbol_count, sizeof(symbol_t));
    symbol_t *symbol = &list->symbols[list->symbol_count - 1];
    symbol->name = name;
    symbol->full_name = full_name;
    symbol->type = symbol_type;
    return symbol;
}

symbol_list_t symbol_list_make() {
    return (symbol_list_t) { .symbol_count = 0, .symbols = NULL };
}

void symbol_list_free(symbol_list_t list) {
    free(list.symbols);
}

symbol_t *symbol_list_find(symbol_list_t *list, const char *name) {
    for(size_t i = 0; i < list->symbol_count; i++) {
        if(strcmp(name, list->symbols[i].name) != 0) continue;
        return &list->symbols[i];
    }
    return NULL;
}

symbol_t *symbol_list_find_type(symbol_list_t *list, const char *name, symbol_type_t type) {
    symbol_t *symbol = symbol_list_find(list, name);
    if(symbol == NULL || symbol->type != type) return NULL;
    return symbol;
}

symbol_t *symbol_list_add_function(symbol_list_t *list, const char *name, const char *full_name, ir_function_t *prototype, LLVMTypeRef type, LLVMValueRef value) {
    symbol_t *symbol = add_symbol(list, name, full_name, SYMBOL_TYPE_FUNCTION);
    symbol->function.prototype = prototype;
    symbol->function.type = type;
    symbol->function.value = value;
    return symbol;
}

symbol_t *symbol_list_add_module(symbol_list_t *list, const char *name, symbol_t *parent) {
    symbol_t *symbol = add_symbol(list, name, name, SYMBOL_TYPE_MODULE);
    symbol->module.parent = parent;
    symbol->module.symbols = SYMBOL_LIST_INIT;
    return symbol;
}