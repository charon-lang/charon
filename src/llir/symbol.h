#pragma once

#include "llir/namespace.h"
#include "llir/type.h"

typedef enum {
    LLIR_SYMBOL_KIND_MODULE,
    LLIR_SYMBOL_KIND_FUNCTION
} llir_symbol_kind_t;

typedef struct llir_symbol llir_symbol_t;

struct llir_symbol {
    const char *name;
    struct llir_namespace *namespace;
    llir_symbol_kind_t kind;
    union {
        struct {
            struct llir_namespace *namespace;
        } module;
        struct {
            llir_type_t *type;
            void *codegen_data;
        } function;
    };
};