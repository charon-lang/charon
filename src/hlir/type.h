#pragma once

#include <stddef.h>

typedef enum {
    HLIR_TYPE_KIND_VOID,
    HLIR_TYPE_KIND_INTEGER,
    HLIR_TYPE_KIND_POINTER,
    HLIR_TYPE_KIND_TUPLE,
    HLIR_TYPE_KIND_ARRAY,
    HLIR_TYPE_KIND_STRUCTURE,
    HLIR_TYPE_KIND_FUNCTION
} hlir_type_kind_t;

typedef struct hlir_type hlir_type_t;

struct hlir_type {
    bool is_reference;
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
                    hlir_type_t **members;
                } structure;
                struct {
                    hlir_type_t *return_type;
                    size_t argument_count;
                    hlir_type_t **arguments;
                    bool varargs;
                } function;
            };
        };
        struct {
            const char *type_name;
        } reference;
    };
};

hlir_type_t *hlir_type_reference_make(const char *type_name);

hlir_type_t *hlir_type_get_void();
hlir_type_t *hlir_type_get_bool();
hlir_type_t *hlir_type_get_char();
hlir_type_t *hlir_type_get_ptr();

hlir_type_t *hlir_type_get_uint();
hlir_type_t *hlir_type_get_u8();
hlir_type_t *hlir_type_get_u16();
hlir_type_t *hlir_type_get_u32();
hlir_type_t *hlir_type_get_u64();

hlir_type_t *hlir_type_get_int();
hlir_type_t *hlir_type_get_i8();
hlir_type_t *hlir_type_get_i16();
hlir_type_t *hlir_type_get_i32();
hlir_type_t *hlir_type_get_i64();

hlir_type_t *hlir_type_pointer_make(hlir_type_t *pointee);
hlir_type_t *hlir_type_tuple_make(size_t type_count, hlir_type_t **types);
hlir_type_t *hlir_type_array_make(hlir_type_t *type, size_t size);
hlir_type_t *hlir_type_structure_make(size_t member_count, hlir_type_t **members);
hlir_type_t *hlir_type_function_make(size_t argument_count, hlir_type_t **arguments, bool varargs, hlir_type_t *return_type);