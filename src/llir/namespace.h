#pragma once

#include "llir/symbol.h"
#include "llir/type.h"

#include <stddef.h>

typedef struct llir_namespace llir_namespace_t;

struct llir_namespace {
    llir_symbol_t *parent;

    size_t symbol_count;
    llir_symbol_t **symbols;

    size_t type_count;
    struct {
        const char *name;
        llir_type_t type;
    } **types;
};

llir_namespace_t *llir_namespace_make(llir_symbol_t *parent);

llir_symbol_t *llir_namespace_find_symbol(llir_namespace_t *namespace, const char *name);
llir_symbol_t *llir_namespace_find_symbol_with_kind(llir_namespace_t *namespace, const char *name, llir_symbol_kind_t kind);
bool llir_namespace_exists_symbol(llir_namespace_t *namespace, const char *name);
llir_symbol_t *llir_namespace_add_symbol_module(llir_namespace_t *namespace, const char *name);
llir_symbol_t *llir_namespace_add_symbol_function(llir_namespace_t *namespace, const char *name, const char *link_name, llir_type_function_t *function_type);
llir_symbol_t *llir_namespace_add_symbol_variable(llir_namespace_t *namespace, const char *name, llir_type_t *type);
llir_symbol_t *llir_namespace_add_symbol_enumeration(llir_namespace_t *namespace, const char *name, llir_type_t *type, const char **members);

llir_type_t *llir_namespace_find_type(llir_namespace_t *namespace, const char *name);
bool llir_namespace_exists_type(llir_namespace_t *namespace, const char *name);
void llir_namespace_add_type(llir_namespace_t *namespace, const char *name);
llir_type_t *llir_namespace_update_type(llir_namespace_t *namespace, const char *name, llir_type_t type);