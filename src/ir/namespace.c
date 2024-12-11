#include "ir.h"
#include "lib/alloc.h"

#include <assert.h>
#include <string.h>

ir_symbol_t *ir_namespace_find_symbol(ir_namespace_t *namespace, const char *name) {
    for(size_t i = 0; i < namespace->symbol_count; i++) {
        ir_symbol_t *symbol = &namespace->symbols[i];
        switch(symbol->kind) {
            case IR_SYMBOL_KIND_MODULE:
                if(strcmp(name, symbol->module->name) != 0) continue;
                break;
            case IR_SYMBOL_KIND_FUNCTION:
                if(strcmp(name, symbol->function->name) != 0) continue;
                break;
            case IR_SYMBOL_KIND_VARIABLE:
                if(strcmp(name, symbol->variable->name) != 0) continue;
                break;
            case IR_SYMBOL_KIND_ENUMERATION:
                if(strcmp(name, symbol->enumeration->name) != 0) continue;
                break;
        }
        return symbol;
    }
    return NULL;
}

ir_symbol_t *ir_namespace_find_symbol_of_kind(ir_namespace_t *namespace, const char *name, ir_symbol_kind_t kind) {
    ir_symbol_t *symbol = ir_namespace_find_symbol(namespace, name);
    if(symbol == NULL || symbol->kind != kind) return NULL;
    return symbol;
}

bool ir_namespace_exists_symbol(ir_namespace_t *namespace, const char *name) {
    return ir_namespace_find_symbol(namespace, name) != NULL;
}

ir_symbol_t *ir_namespace_add_symbol(ir_namespace_t *namespace, ir_symbol_kind_t kind) {
    namespace->symbols = alloc_array(namespace->symbols, ++namespace->symbol_count, sizeof(ir_symbol_t));
    namespace->symbols[namespace->symbol_count - 1].kind = kind;
    return &namespace->symbols[namespace->symbol_count - 1];
}

ir_type_declaration_t *ir_namespace_find_type(ir_namespace_t *namespace, const char *name) {
    for(size_t i = 0; i < namespace->type_count; i++) {
        if(strcmp(name, namespace->types[i].name) != 0) continue;
        return &namespace->types[i];
    }
    return NULL;
}

bool ir_namespace_exists_type(ir_namespace_t *namespace, const char *name) {
    return ir_namespace_find_type(namespace, name) != NULL;
}

void ir_namespace_add_type(ir_namespace_t *namespace, const char *name, ir_type_t *type) {
    namespace->types = alloc_array(namespace->types, ++namespace->type_count, sizeof(ir_type_declaration_t));
    namespace->types[namespace->type_count - 1] = (ir_type_declaration_t) { .name = name, .type = type };
}