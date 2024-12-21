#include "ir.h"
#include "lib/alloc.h"

#include <assert.h>
#include <string.h>

enum {
    PRIMITIVE_U64,
    PRIMITIVE_I64,
    PRIMITIVE_U8,
    PRIMITIVE_U32,
    PRIMITIVE_I32,
    PRIMITIVE_U1,
    PRIMITIVE_VOID,
    PRIMITIVE_U16,
    PRIMITIVE_I16,
    PRIMITIVE_I8,

    PRIMITIVE_COUNT
};

static ir_type_t *make_type(ir_type_kind_t kind) {
    ir_type_t *type = alloc(sizeof(ir_type_t));
    type->kind = kind;
    return type;
}

static ir_type_t *make_type_integer(size_t bit_size, bool is_signed, bool allow_coerce_pointer) {
    ir_type_t *type = make_type(IR_TYPE_KIND_INTEGER);
    type->integer.bit_size = bit_size;
    type->integer.is_signed = is_signed;
    type->integer.allow_coerce_pointer = allow_coerce_pointer;
    return type;
}

static void cache_add(ir_type_cache_t *cache, ir_type_t *type) {
    cache->types = alloc_array(cache->types, ++cache->type_count, sizeof(ir_type_t *));
    cache->types[cache->type_count - 1] = type;
}

bool ir_type_eq(ir_type_t *a, ir_type_t *b) {
    if(
        a->kind == IR_TYPE_KIND_INTEGER && b->kind == IR_TYPE_KIND_INTEGER
        && a->integer.bit_size == b->integer.bit_size
        && a->integer.is_signed == b->integer.is_signed
        && a->integer.allow_coerce_pointer == b->integer.allow_coerce_pointer
    ) return true;
    return a == b;
}

ir_type_cache_t *ir_type_cache_make() {
    ir_type_cache_t *cache = alloc(sizeof(ir_type_cache_t));
    cache->type_count = PRIMITIVE_COUNT;
    cache->types = alloc_array(NULL, cache->type_count, sizeof(ir_type_t *));
    cache->defined_type_count = 0;
    cache->defined_types = NULL;

    cache->types[PRIMITIVE_VOID] = make_type(IR_TYPE_KIND_VOID);
    cache->types[PRIMITIVE_U1] = make_type_integer(1, false, false);
    cache->types[PRIMITIVE_U8] = make_type_integer(8, false, false);
    cache->types[PRIMITIVE_U16] = make_type_integer(16, false, false);
    cache->types[PRIMITIVE_U32] = make_type_integer(32, false, false);
    cache->types[PRIMITIVE_U64] = make_type_integer(64, false, false);
    cache->types[PRIMITIVE_I8] = make_type_integer(8, true, false);
    cache->types[PRIMITIVE_I16] = make_type_integer(16, true, false);
    cache->types[PRIMITIVE_I32] = make_type_integer(32, true, false);
    cache->types[PRIMITIVE_I64] = make_type_integer(64, true, false);

    return cache;
}

ir_type_t *ir_type_cache_get_void(ir_type_cache_t *cache) {
    return cache->types[PRIMITIVE_VOID];
}

ir_type_t *ir_type_cache_get_integer(ir_type_cache_t *cache, size_t bit_size, bool is_signed, bool allow_coerce_pointer) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != IR_TYPE_KIND_INTEGER) continue;
        if(cache->types[i]->integer.bit_size != bit_size || cache->types[i]->integer.is_signed != is_signed || cache->types[i]->integer.allow_coerce_pointer != allow_coerce_pointer) continue;
        return cache->types[i];
    }
    ir_type_t *type = make_type_integer(bit_size, is_signed, allow_coerce_pointer);
    cache_add(cache, type);
    return type;
}

ir_type_t *ir_type_cache_get_pointer(ir_type_cache_t *cache, ir_type_t *pointee) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != IR_TYPE_KIND_POINTER) continue;
        if(!ir_type_eq(cache->types[i]->pointer.pointee, pointee)) continue;
        return cache->types[i];
    }
    ir_type_t *type = make_type(IR_TYPE_KIND_POINTER);
    type->pointer.pointee = pointee;
    cache_add(cache, type);
    return type;
}

ir_type_t *ir_type_cache_get_tuple(ir_type_cache_t *cache, size_t type_count, ir_type_t **types) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != IR_TYPE_KIND_TUPLE) continue;
        if(cache->types[i]->tuple.type_count != type_count) continue;
        for(size_t j = 0; j < cache->types[i]->tuple.type_count; j++) if(!ir_type_eq(cache->types[i]->tuple.types[j], types[j])) goto cont;
        return cache->types[i];
        cont:
    }
    ir_type_t *type = make_type(IR_TYPE_KIND_TUPLE);
    type->tuple.type_count = type_count;
    type->tuple.types = types;
    cache_add(cache, type);
    return type;
}

ir_type_t *ir_type_cache_get_array(ir_type_cache_t *cache, ir_type_t *element_type, size_t size) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != IR_TYPE_KIND_ARRAY) continue;
        if(cache->types[i]->array.size != size) continue;
        if(!ir_type_eq(cache->types[i]->array.type, element_type)) continue;
        return cache->types[i];
    }
    ir_type_t *type = make_type(IR_TYPE_KIND_ARRAY);
    type->array.type = element_type;
    type->array.size = size;
    cache_add(cache, type);
    return type;
}

ir_type_t *ir_type_cache_get_structure(ir_type_cache_t *cache, size_t member_count, ir_type_structure_member_t *members, bool packed) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != IR_TYPE_KIND_STRUCTURE) continue;
        if(cache->types[i]->structure.packed != packed) continue;
        if(cache->types[i]->structure.member_count != member_count) continue;
        for(size_t j = 0; j < cache->types[i]->structure.member_count; j++) {
            if(strcmp(cache->types[i]->structure.members[j].name, members[j].name) != 0) goto cont;
            if(!ir_type_eq(cache->types[i]->structure.members[j].type, members[j].type)) goto cont;
        }
        return cache->types[i];
        cont:
    }
    ir_type_t *type = make_type(IR_TYPE_KIND_STRUCTURE);
    type->structure.packed = packed;
    type->structure.member_count = member_count;
    type->structure.members = members;
    cache_add(cache, type);
    return type;
}

ir_type_t *ir_type_cache_get_function_reference(ir_type_cache_t *cache, ir_function_prototype_t prototype) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != IR_TYPE_KIND_FUNCTION_REFERENCE) continue;
        if(cache->types[i]->function_reference.prototype.varargs != prototype.varargs) continue;
        if(cache->types[i]->function_reference.prototype.argument_count != prototype.argument_count) continue;
        if(!ir_type_eq(cache->types[i]->function_reference.prototype.return_type, prototype.return_type)) continue;
        for(size_t j = 0; j < prototype.argument_count; j++) {
            if(!ir_type_eq(cache->types[i]->function_reference.prototype.arguments[j], prototype.arguments[j])) goto cont;
        }
        return cache->types[i];
        cont:
    }
    ir_type_t *type = make_type(IR_TYPE_KIND_FUNCTION_REFERENCE);
    type->function_reference.prototype = prototype;
    cache_add(cache, type);
    return type;
}

ir_type_t *ir_type_cache_get_enumeration(ir_type_cache_t *cache, size_t bit_size, size_t member_count) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != IR_TYPE_KIND_ENUMERATION) continue;
        if(cache->types[i]->enumeration.bit_size != bit_size) continue;
        if(cache->types[i]->enumeration.member_count != member_count) continue;
        return cache->types[i];
    }
    ir_type_t *type = make_type(IR_TYPE_KIND_ENUMERATION);
    type->enumeration.bit_size = bit_size;
    type->enumeration.member_count = member_count;
    cache_add(cache, type);
    return type;
}

ir_type_t *ir_type_cache_define(ir_type_cache_t *cache) {
    cache->types = alloc_array(cache->types, ++cache->type_count, sizeof(ir_type_t *));
    cache->types[cache->type_count - 1] = alloc(sizeof(ir_type_t));
    return cache->types[cache->type_count - 1];
}