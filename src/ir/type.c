#include "type.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

ir_type_t *ir_type_pointer_make(ir_type_t *referred) {
    ir_type_t *type = make_type(IR_TYPE_KIND_POINTER);
    type->pointer.referred = referred;
    return type;
}

ir_type_t *ir_type_tuple_make(size_t type_count, ir_type_t **types) {
    ir_type_t *type = make_type(IR_TYPE_KIND_TUPLE);
    type->tuple.type_count = type_count;
    type->tuple.types = types;
    return type;
}

ir_type_t *ir_type_array_make(ir_type_t *type, size_t size) {
    ir_type_t *new_type = make_type(IR_TYPE_KIND_ARRAY);
    new_type->array.type = type;
    new_type->array.size = size;
    return new_type;
}

ir_type_t *ir_type_function_make() {
    ir_type_t *type = make_type(IR_TYPE_KIND_FUNCTION);
    type->function.return_type = ir_type_get_void();
    type->function.varargs = false;
    type->function.argument_count = 0;
    type->function.arguments = NULL;
    return type;
}

void ir_type_function_set_varargs(ir_type_t *fn_type) {
    fn_type->function.varargs = true;
}

void ir_type_function_set_return_type(ir_type_t *fn_type, ir_type_t *return_type) {
    fn_type->function.return_type = return_type;
}

void ir_type_function_add_argument(ir_type_t *fn_type, ir_type_t *argument_type) {
    fn_type->function.arguments = reallocarray(fn_type->function.arguments, ++fn_type->function.argument_count, sizeof(ir_type_t *));
    fn_type->function.arguments[fn_type->function.argument_count - 1] = argument_type;
}

bool ir_type_eq(ir_type_t *a, ir_type_t *b) {
    if(a->kind != b->kind) return false;
    switch(a->kind) {
        case IR_TYPE_KIND_VOID: return true;
        case IR_TYPE_KIND_INTEGER: return a->integer.bit_size == b->integer.bit_size && a->integer.is_signed == b->integer.is_signed;
        case IR_TYPE_KIND_POINTER: return ir_type_eq(a->pointer.referred, b->pointer.referred);
        case IR_TYPE_KIND_TUPLE:
            if(a->tuple.type_count != b->tuple.type_count) return false;
            for(size_t i = 0; i < a->tuple.type_count; i++) if(!ir_type_eq(a->tuple.types[i], b->tuple.types[i])) return false;
            return true;
        case IR_TYPE_KIND_ARRAY:
            if(a->array.size != b->array.size) return false;
            return ir_type_eq(a->array.type, b->array.type);
        case IR_TYPE_KIND_FUNCTION:
            if(a->function.varargs != b->function.varargs) return false;
            if(a->function.argument_count != b->function.argument_count) return false;
            for(size_t i = 0; i < a->function.argument_count; i++) if(!ir_type_eq(a->function.arguments[i], b->function.arguments[i])) return false;
            return ir_type_eq(a->function.return_type, b->function.return_type);
    }
    assert(false);
}

ir_type_t *ir_type_get_void() {
    if(g_void == NULL) g_void = make_type(IR_TYPE_KIND_VOID);
    return g_void;
}

ir_type_t *ir_type_get_bool() {
    if(g_bool == NULL) g_bool = make_int_type(1, false);
    return g_bool;
}

ir_type_t *ir_type_get_char() {
    return ir_type_get_u8();
}

ir_type_t *ir_type_get_ptr() {
    return ir_type_get_u64();
}

ir_type_t *ir_type_get_uint() {
    return ir_type_get_u64();
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

ir_type_t *ir_type_get_int() {
    return ir_type_get_i64();
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