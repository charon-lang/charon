#include "type.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static ir_type_t
    *g_void = NULL,
    *g_u8 = NULL,
    *g_u16 = NULL,
    *g_u32 = NULL,
    *g_u64 = NULL,
    *g_i8 = NULL,
    *g_i16 = NULL,
    *g_i32 = NULL,
    *g_i64 = NULL,
    *g_bool = NULL;

static ir_type_t *make_type(ir_type_kind_t kind) {
    ir_type_t *type = malloc(sizeof(ir_type_t));
    type->kind = kind;
    return type;
}

static ir_type_t *make_int_type(size_t bit_size, bool is_signed) {
    ir_type_t *type = make_type(IR_TYPE_KIND_INTEGER);
    type->integer.bit_size = bit_size,
    type->integer.is_signed = is_signed;
    return type;
}

bool ir_type_is_kind(ir_type_t *type, ir_type_kind_t kind) {
    return type->kind == kind;
}

bool ir_type_is_void(ir_type_t *type) {
    return ir_type_is_kind(type, IR_TYPE_KIND_VOID);
}

int ir_type_cmp(ir_type_t *a, ir_type_t *b) {
    if(a->kind != b->kind) return a->kind - b->kind;
    switch(a->kind) {
        case IR_TYPE_KIND_VOID: return 0;
        case IR_TYPE_KIND_INTEGER: return a->integer.bit_size - b->integer.bit_size;
        case IR_TYPE_KIND_POINTER: return ir_type_cmp(a->pointer.base, b->pointer.base);
    }
    assert(false);
}

ir_type_t *ir_type_get_void() {
    if(g_void == NULL) g_void = make_type(IR_TYPE_KIND_VOID);
    return g_void;
}

ir_type_t *ir_type_get_u8() {
    if(g_u8 == NULL) g_u8 = make_int_type(8, false);
    return g_u8;
}

ir_type_t *ir_type_get_u16() {
    if(g_u16 == NULL) g_u16 = make_int_type(16, false);
    return g_u16;
}

ir_type_t *ir_type_get_u32() {
    if(g_u32 == NULL) g_u32 = make_int_type(32, false);
    return g_u32;
}

ir_type_t *ir_type_get_u64() {
    if(g_u64 == NULL) g_u64 = make_int_type(64, false);
    return g_u64;
}

ir_type_t *ir_type_get_uint() {
    return ir_type_get_u64();
}

ir_type_t *ir_type_get_i8() {
    if(g_i8 == NULL) g_i8 = make_int_type(8, true);
    return g_i8;
}

ir_type_t *ir_type_get_i16() {
    if(g_i16 == NULL) g_i16 = make_int_type(16, true);
    return g_i16;
}

ir_type_t *ir_type_get_i32() {
    if(g_i32 == NULL) g_i32 = make_int_type(32, true);
    return g_i32;
}

ir_type_t *ir_type_get_i64() {
    if(g_i64 == NULL) g_i64 = make_int_type(64, true);
    return g_i64;
}

ir_type_t *ir_type_get_int() {
    return ir_type_get_i64();
}

ir_type_t *ir_type_get_bool() {
    if(g_bool == NULL) g_bool = make_int_type(1, false);
    return g_bool;
}

ir_type_t *ir_type_make_pointer(ir_type_t *base) {
    ir_type_t *type = make_type(IR_TYPE_KIND_POINTER);
    type->pointer.base = base;
    return type;
}