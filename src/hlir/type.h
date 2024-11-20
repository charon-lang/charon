#pragma once

#include "hlir/attribute.h"

#include <stddef.h>

typedef enum {
    HLIR_TYPE_KIND_VOID,
    HLIR_TYPE_KIND_INTEGER,
    HLIR_TYPE_KIND_POINTER,
    HLIR_TYPE_KIND_TUPLE,
    HLIR_TYPE_KIND_ARRAY,
    HLIR_TYPE_KIND_STRUCTURE,
    HLIR_TYPE_KIND_FUNCTION,
    HLIR_TYPE_KIND_FUNCTION_REFERENCE
} hlir_type_kind_t;

typedef struct hlir_type hlir_type_t;

typedef struct {
    hlir_type_t *type;
    const char *name;
} hlir_type_structure_member_t;

struct hlir_type {
    bool is_reference;
    hlir_attribute_list_t attributes;
    union {
        struct {
            hlir_type_kind_t kind;
            union {
                struct {
                    bool is_signed;
                    size_t bit_size;
                } integer;
                struct {
                    hlir_type_t *pointee;
                } pointer;
                struct {
                    size_t type_count;
                    hlir_type_t **types;
                } tuple;
                struct {
                    hlir_type_t *type;
                    size_t size;
                } array;
                struct {
                    size_t member_count;
                    hlir_type_structure_member_t *members;
                } structure;
                struct {
                    hlir_type_t *return_type;
                    size_t argument_count;
                    hlir_type_t **arguments;
                    bool varargs;
                } function;
                struct {
                    hlir_type_t *function_type;
                } function_reference;
            };
        };
        struct {
            const char *type_name;
        } reference;
    };
};

hlir_type_t *hlir_type_reference_make(const char *type_name, hlir_attribute_list_t attributes);

hlir_type_t *hlir_type_void_make(hlir_attribute_list_t attributes);
hlir_type_t *hlir_type_integer_make(size_t bit_size, bool is_signed, hlir_attribute_list_t attributes);
hlir_type_t *hlir_type_pointer_make(hlir_type_t *pointee, hlir_attribute_list_t attributes);
hlir_type_t *hlir_type_tuple_make(size_t type_count, hlir_type_t **types, hlir_attribute_list_t attributes);
hlir_type_t *hlir_type_array_make(hlir_type_t *type, size_t size, hlir_attribute_list_t attributes);
hlir_type_t *hlir_type_structure_make(size_t member_count, hlir_type_structure_member_t *members, hlir_attribute_list_t attributes);
hlir_type_t *hlir_type_function_make(size_t argument_count, hlir_type_t **arguments, bool varargs, hlir_type_t *return_type, hlir_attribute_list_t attributes);
hlir_type_t *hlir_type_function_reference_make(hlir_type_t *function_type, hlir_attribute_list_t attributes);