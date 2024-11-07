#pragma once

#include <stddef.h>

typedef enum {
    LLIR_TYPE_KIND_VOID,
    LLIR_TYPE_KIND_INTEGER,
    LLIR_TYPE_KIND_POINTER,
    LLIR_TYPE_KIND_TUPLE,
    LLIR_TYPE_KIND_ARRAY,
    LLIR_TYPE_KIND_STRUCTURE,
    LLIR_TYPE_KIND_FUNCTION
} llir_type_kind_t;

typedef struct llir_type llir_type_t;

typedef struct {
    llir_type_t *type;
    const char *name;
} llir_type_structure_member_t;

struct llir_type {
    llir_type_kind_t kind;
    union {
        struct {
            bool is_signed;
            size_t bit_size;
        } integer;
        struct {
            llir_type_t *pointee;
        } pointer;
        struct {
            size_t type_count;
            llir_type_t **types;
        } tuple;
        struct {
            llir_type_t *type;
            size_t size;
        } array;
        struct {
            size_t member_count;
            llir_type_structure_member_t *members;
        } structure;
        struct {
            llir_type_t *return_type;
            size_t argument_count;
            llir_type_t **arguments;
            bool varargs;
        } function;
    };
};

bool llir_type_eq(llir_type_t *a, llir_type_t *b);

llir_type_t *llir_type_tuple_make(size_t type_count, llir_type_t **types);