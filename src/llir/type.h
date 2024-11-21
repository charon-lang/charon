#pragma once

#include <stddef.h>

typedef enum {
    LLIR_TYPE_KIND_VOID,
    LLIR_TYPE_KIND_INTEGER,
    LLIR_TYPE_KIND_POINTER,
    LLIR_TYPE_KIND_TUPLE,
    LLIR_TYPE_KIND_ARRAY,
    LLIR_TYPE_KIND_STRUCTURE,
    LLIR_TYPE_KIND_FUNCTION_REFERENCE,
    LLIR_TYPE_KIND_ENUMERATION
} llir_type_kind_t;

typedef struct llir_type llir_type_t;
typedef struct llir_type_function llir_type_function_t;

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
            bool packed;
            size_t member_count;
            llir_type_structure_member_t *members;
        } structure;
        struct {
            llir_type_function_t *function_type;
        } function_reference;
        struct {
            size_t bit_size;
            size_t member_count;
        } enumeration;
    };
};

struct llir_type_function {
    llir_type_t *return_type;
    size_t argument_count;
    llir_type_t **arguments;
    bool varargs;
};

typedef struct {
    llir_type_t **types;
    size_t type_count;
    llir_type_function_t **function_types;
    size_t function_type_count;
} llir_type_cache_t;

bool llir_type_eq(llir_type_t *a, llir_type_t *b);

llir_type_cache_t *llir_type_cache_make();
void llir_type_cache_free(llir_type_cache_t *cache);

llir_type_t *llir_type_cache_get_void(llir_type_cache_t *cache);
llir_type_t *llir_type_cache_get_integer(llir_type_cache_t *cache, size_t bit_size, bool is_signed);
llir_type_t *llir_type_cache_get_pointer(llir_type_cache_t *cache, llir_type_t *pointee);
llir_type_t *llir_type_cache_get_tuple(llir_type_cache_t *cache, size_t type_count, llir_type_t **types);
llir_type_t *llir_type_cache_get_array(llir_type_cache_t *cache, llir_type_t *element_type, size_t size);
llir_type_t *llir_type_cache_get_structure(llir_type_cache_t *cache, size_t member_count, llir_type_structure_member_t *members, bool packed);
llir_type_t *llir_type_cache_get_function_reference(llir_type_cache_t *cache, llir_type_function_t *function_type);
llir_type_t *llir_type_cache_get_enumeration(llir_type_cache_t *cache, size_t bit_size, size_t member_count);

llir_type_function_t *llir_type_cache_get_function_type(llir_type_cache_t *cache, size_t argument_count, llir_type_t **arguments, bool varargs, llir_type_t *return_type);