#pragma once
#include <stddef.h>

typedef enum {
    TYPE_KIND_VOID,
    TYPE_KIND_POINTER,
    TYPE_KIND_UNSIGNED_INTEGER
} ir_type_kind_t;

typedef struct type {
    ir_type_kind_t kind;
    size_t bit_size;
    union {
        struct {
            struct type *base;
        } pointer;
    };
} ir_type_t;

bool ir_type_is_kind(ir_type_t *type, ir_type_kind_t kind);
bool ir_type_is_void(ir_type_t *type);

ir_type_t *ir_type_get_void();
ir_type_t *ir_type_get_u8();
ir_type_t *ir_type_get_u16();
ir_type_t *ir_type_get_u32();
ir_type_t *ir_type_get_u64();
ir_type_t *ir_type_get_uint();

ir_type_t *ir_type_make_pointer(ir_type_t *base);

void ir_type_print(ir_type_t *type);