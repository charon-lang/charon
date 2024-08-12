#pragma once

#include <stddef.h>

typedef enum {
    IR_TYPE_KIND_VOID,
    IR_TYPE_KIND_INTEGER,
    IR_TYPE_KIND_POINTER,
    IR_TYPE_KIND_TUPLE,
    IR_TYPE_KIND_ARRAY
} ir_type_kind_t;

typedef struct ir_type {
    ir_type_kind_t kind;
    union {
        struct {
            bool is_signed;
            size_t bit_size;
        } integer;
        struct {
            struct ir_type *referred;
        } pointer;
        struct {
            size_t type_count;
            struct ir_type **types;
        } tuple;
        struct {
            struct ir_type *type;
            size_t size;
        } array;
    };
} ir_type_t;

bool ir_type_eq(ir_type_t *a, ir_type_t *b);

ir_type_t *ir_type_pointer_make(ir_type_t *referred);

ir_type_t *ir_type_tuple_make(size_t type_count, ir_type_t **types);

ir_type_t *ir_type_array_make(ir_type_t *type, size_t size);

ir_type_t *ir_type_get_void();
ir_type_t *ir_type_get_bool();
ir_type_t *ir_type_get_char();

ir_type_t *ir_type_get_uint();
ir_type_t *ir_type_get_u8();
ir_type_t *ir_type_get_u16();
ir_type_t *ir_type_get_u32();
ir_type_t *ir_type_get_u64();

ir_type_t *ir_type_get_int();
ir_type_t *ir_type_get_i8();
ir_type_t *ir_type_get_i16();
ir_type_t *ir_type_get_i32();
ir_type_t *ir_type_get_i64();