#include "type.h"
#include <stdlib.h>
#include <stdio.h>

static ir_type_t *g_void = NULL, *g_u8 = NULL, *g_u16 = NULL, *g_u32 = NULL, *g_u64 = NULL;

static ir_type_t *make_type(ir_type_kind_t kind, size_t bit_size) {
    ir_type_t *type = malloc(sizeof(ir_type_t));
    type->kind = kind;
    type->bit_size = bit_size;
    return type;
}

bool ir_type_is_kind(ir_type_t *type, ir_type_kind_t kind) {
    return type->kind == kind;
}

bool ir_type_is_void(ir_type_t *type) {
    return ir_type_is_kind(type, TYPE_KIND_VOID);
}

ir_type_t *ir_type_get_void() {
    if(g_void == NULL) g_void = make_type(TYPE_KIND_VOID, 0);
    return g_void;
}

ir_type_t *ir_type_get_u8() {
    if(g_u8 == NULL) g_u8 = make_type(TYPE_KIND_UNSIGNED_INTEGER, 8);
    return g_u8;
}

ir_type_t *ir_type_get_u16() {
    if(g_u16 == NULL) g_u16 = make_type(TYPE_KIND_UNSIGNED_INTEGER, 16);
    return g_u16;
}

ir_type_t *ir_type_get_u32() {
    if(g_u32 == NULL) g_u32 = make_type(TYPE_KIND_UNSIGNED_INTEGER, 32);
    return g_u32;
}

ir_type_t *ir_type_get_u64() {
    if(g_u64 == NULL) g_u64 = make_type(TYPE_KIND_UNSIGNED_INTEGER, 64);
    return g_u64;
}

ir_type_t *ir_type_get_uint() {
    return ir_type_get_u64();
}

ir_type_t *ir_type_make_pointer(ir_type_t *base) {
    ir_type_t *type = make_type(TYPE_KIND_POINTER, 64);
    type->pointer.base = base;
    return type;
}

void ir_type_print(ir_type_t *type) {
    switch(type->kind) {
        case TYPE_KIND_VOID: printf("void"); break;
        case TYPE_KIND_UNSIGNED_INTEGER: printf("%lubit-uint", type->bit_size); break;
        case TYPE_KIND_POINTER: printf("*"); ir_type_print(type->pointer.base); break;
    }
}