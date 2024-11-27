#include "namespace.h"

#include "lib/alloc.h"

#include <assert.h>
#include <string.h>

static llir_symbol_t *add_symbol(llir_namespace_t *namespace, const char *name, llir_symbol_kind_t kind) {
    assert(!llir_namespace_exists_symbol(namespace, name));
    llir_symbol_t *symbol = alloc(sizeof(llir_symbol_t));
    symbol->name = name;
    symbol->kind = kind;
    symbol->namespace = namespace;
    namespace->symbols = alloc_array(namespace->symbols, ++namespace->symbol_count, sizeof(llir_symbol_t *));
    namespace->symbols[namespace->symbol_count - 1] = symbol;
    return symbol;
}

llir_namespace_t *llir_namespace_make(llir_symbol_t *parent) {
    llir_namespace_t *namespace = alloc(sizeof(llir_namespace_t));
    namespace->parent = parent;
    namespace->symbol_count = 0;
    namespace->symbols = NULL;
    namespace->type_count = 0;
    namespace->types = NULL;
    return namespace;
}

llir_symbol_t *llir_namespace_find_symbol(llir_namespace_t *namespace, const char *name) {
    for(size_t i = 0; i < namespace->symbol_count; i++) {
        if(strcmp(name, namespace->symbols[i]->name) != 0) continue;
        return namespace->symbols[i];
    }
    return NULL;
}

llir_symbol_t *llir_namespace_find_symbol_with_kind(llir_namespace_t *namespace, const char *name, llir_symbol_kind_t kind) {
    llir_symbol_t *symbol = llir_namespace_find_symbol(namespace, name);
    if(symbol == NULL || symbol->kind != kind) return NULL;
    return symbol;
}

bool llir_namespace_exists_symbol(llir_namespace_t *namespace, const char *name) {
    return llir_namespace_find_symbol(namespace, name) != NULL;
}

llir_symbol_t *llir_namespace_add_symbol_module(llir_namespace_t *namespace, const char *name) {
    llir_symbol_t *symbol = add_symbol(namespace, name, LLIR_SYMBOL_KIND_MODULE);
    symbol->module.namespace = llir_namespace_make(symbol);
    return symbol;
}

llir_symbol_t *llir_namespace_add_symbol_function(llir_namespace_t *namespace, const char *name, const char *link_name, llir_type_function_t *function_type)  {
    llir_symbol_t *symbol = add_symbol(namespace, name, LLIR_SYMBOL_KIND_FUNCTION);
    symbol->function.link_name = link_name;
    symbol->function.function_type = function_type;
    symbol->function.codegen_data = NULL;
    return symbol;
}

llir_symbol_t *llir_namespace_add_symbol_variable(llir_namespace_t *namespace, const char *name, llir_type_t *type) {
    llir_symbol_t *symbol = add_symbol(namespace, name, LLIR_SYMBOL_KIND_VARIABLE);
    symbol->variable.type = type;
    symbol->variable.codegen_data = NULL;
    return symbol;
}

llir_symbol_t *llir_namespace_add_symbol_enumeration(llir_namespace_t *namespace, const char *name, llir_type_t *type, const char **members) {
    llir_symbol_t *symbol = add_symbol(namespace, name, LLIR_SYMBOL_KIND_ENUMERATION);
    symbol->enumeration.type = type;
    symbol->enumeration.members = members;
    return symbol;
}

llir_type_t *llir_namespace_find_type(llir_namespace_t *namespace, const char *name) {
    for(size_t i = 0; i < namespace->type_count; i++) {
        if(strcmp(name, namespace->types[i]->name) != 0) continue;
        return &namespace->types[i]->type;
    }
    return NULL;
}

bool llir_namespace_exists_type(llir_namespace_t *namespace, const char *name) {
    return llir_namespace_find_type(namespace, name) != NULL;
}

void llir_namespace_add_type(llir_namespace_t *namespace, const char *name) {
    assert(!llir_namespace_exists_type(namespace, name));

    typeof(namespace->types[0]) entry = alloc(sizeof(namespace->types[0][0]));
    entry->name = name;

    namespace->types = alloc_array(namespace->types, ++namespace->type_count, sizeof(namespace->types[0]));
    namespace->types[namespace->type_count - 1] = entry;
}

llir_type_t *llir_namespace_update_type(llir_namespace_t *namespace, const char *name, llir_type_t type) {
    llir_type_t *p = llir_namespace_find_type(namespace, name);
    assert(p != NULL);
    *p = type;
    return p;
}