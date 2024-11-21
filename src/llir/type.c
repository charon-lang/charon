#include "type.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

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

static llir_type_t *make_type(llir_type_kind_t kind) {
    llir_type_t *type = malloc(sizeof(llir_type_t));
    type->kind = kind;
    return type;
}

static llir_type_t *make_type_integer(size_t bit_size, bool is_signed) {
    llir_type_t *type = make_type(LLIR_TYPE_KIND_INTEGER);
    type->integer.bit_size = bit_size;
    type->integer.is_signed = is_signed;
    return type;
}

static void cache_add(llir_type_cache_t *cache, llir_type_t *type) {
    cache->types = reallocarray(cache->types, ++cache->type_count, sizeof(llir_type_t *));
    cache->types[cache->type_count - 1] = type;
}

bool llir_type_eq(llir_type_t *a, llir_type_t *b) {
    return a == b;
}

static bool llir_type_function_eq(llir_type_function_t *a, llir_type_function_t *b) {
    return a == b;
}

llir_type_cache_t *llir_type_cache_make() {
    llir_type_cache_t *cache = malloc(sizeof(llir_type_cache_t));
    cache->function_type_count = 0;
    cache->function_types = NULL;
    cache->type_count = PRIMITIVE_COUNT;
    cache->types = reallocarray(NULL, cache->type_count, sizeof(llir_type_t *));

    cache->types[PRIMITIVE_VOID] = make_type(LLIR_TYPE_KIND_VOID);
    cache->types[PRIMITIVE_U1] = make_type_integer(1, false);
    cache->types[PRIMITIVE_U8] = make_type_integer(8, false);
    cache->types[PRIMITIVE_U16] = make_type_integer(16, false);
    cache->types[PRIMITIVE_U32] = make_type_integer(32, false);
    cache->types[PRIMITIVE_U64] = make_type_integer(64, false);
    cache->types[PRIMITIVE_I8] = make_type_integer(8, true);
    cache->types[PRIMITIVE_I16] = make_type_integer(16, true);
    cache->types[PRIMITIVE_I32] = make_type_integer(32, true);
    cache->types[PRIMITIVE_I64] = make_type_integer(64, true);

    return cache;
}

void llir_type_cache_free(llir_type_cache_t *cache) {
    if(cache->function_types != NULL) free(cache->function_types);
    if(cache->types != NULL) free(cache->types);
    free(cache);
}

llir_type_t *llir_type_cache_get_void(llir_type_cache_t *cache) {
    return cache->types[PRIMITIVE_VOID];
}

llir_type_t *llir_type_cache_get_integer(llir_type_cache_t *cache, size_t bit_size, bool is_signed) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != LLIR_TYPE_KIND_INTEGER) continue;
        if(cache->types[i]->integer.bit_size != bit_size || cache->types[i]->integer.is_signed != is_signed) continue;
        return cache->types[i];
    }
    llir_type_t *type = make_type_integer(bit_size, is_signed);
    cache_add(cache, type);
    return type;
}

llir_type_t *llir_type_cache_get_pointer(llir_type_cache_t *cache, llir_type_t *pointee) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != LLIR_TYPE_KIND_POINTER) continue;
        if(!llir_type_eq(cache->types[i]->pointer.pointee, pointee)) continue;
        return cache->types[i];
    }
    llir_type_t *type = make_type(LLIR_TYPE_KIND_POINTER);
    type->pointer.pointee = pointee;
    cache_add(cache, type);
    return type;
}

llir_type_t *llir_type_cache_get_tuple(llir_type_cache_t *cache, size_t type_count, llir_type_t **types) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != LLIR_TYPE_KIND_TUPLE) continue;
        if(cache->types[i]->tuple.type_count != type_count) continue;
        for(size_t j = 0; j < cache->types[i]->tuple.type_count; j++) if(!llir_type_eq(cache->types[i]->tuple.types[j], types[j])) goto cont;
        return cache->types[i];
        cont:
    }
    llir_type_t *type = make_type(LLIR_TYPE_KIND_TUPLE);
    type->tuple.type_count = type_count;
    type->tuple.types = types;
    cache_add(cache, type);
    return type;
}

llir_type_t *llir_type_cache_get_array(llir_type_cache_t *cache, llir_type_t *element_type, size_t size) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != LLIR_TYPE_KIND_ARRAY) continue;
        if(cache->types[i]->array.size != size) continue;
        if(!llir_type_eq(cache->types[i]->array.type, element_type)) continue;
        return cache->types[i];
    }
    llir_type_t *type = make_type(LLIR_TYPE_KIND_ARRAY);
    type->array.type = element_type;
    type->array.size = size;
    cache_add(cache, type);
    return type;
}

llir_type_t *llir_type_cache_get_structure(llir_type_cache_t *cache, size_t member_count, llir_type_structure_member_t *members, bool packed) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != LLIR_TYPE_KIND_STRUCTURE) continue;
        if(cache->types[i]->structure.packed != packed) continue;
        if(cache->types[i]->structure.member_count != member_count) continue;
        for(size_t j = 0; j < cache->types[i]->structure.member_count; j++) {
            if(strcmp(cache->types[i]->structure.members[j].name, members[j].name) != 0) goto cont;
            if(!llir_type_eq(cache->types[i]->structure.members[j].type, members[j].type)) goto cont;
        }
        return cache->types[i];
        cont:
    }
    llir_type_t *type = make_type(LLIR_TYPE_KIND_STRUCTURE);
    type->structure.packed = packed;
    type->structure.member_count = member_count;
    type->structure.members = members;
    cache_add(cache, type);
    return type;
}

llir_type_t *llir_type_cache_get_function_reference(llir_type_cache_t *cache, llir_type_function_t *function_type) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != LLIR_TYPE_KIND_FUNCTION_REFERENCE) continue;
        if(!llir_type_function_eq(cache->types[i]->function_reference.function_type, function_type)) continue;
        return cache->types[i];
    }
    llir_type_t *type = make_type(LLIR_TYPE_KIND_FUNCTION_REFERENCE);
    type->function_reference.function_type = function_type;
    cache_add(cache, type);
    return type;
}

llir_type_function_t *llir_type_cache_get_function_type(llir_type_cache_t *cache, size_t argument_count, llir_type_t **arguments, bool varargs, llir_type_t *return_type) {
    for(size_t i = 0; i < cache->function_type_count; i++) {
        if(cache->function_types[i]->varargs != varargs) continue;
        if(cache->function_types[i]->argument_count != argument_count) continue;
        if(!llir_type_eq(cache->function_types[i]->return_type, return_type)) continue;
        for(size_t j = 0; j < cache->function_types[i]->argument_count; j++) if(!llir_type_eq(cache->function_types[i]->arguments[j], arguments[j])) goto cont;
        return cache->function_types[i];
        cont:
    }
    llir_type_function_t *function_type = malloc(sizeof(llir_type_function_t));
    function_type->varargs = varargs;
    function_type->argument_count = argument_count;
    function_type->return_type = return_type;
    function_type->arguments = arguments;
    cache->function_types = reallocarray(cache->function_types, ++cache->function_type_count, sizeof(llir_type_function_t *));
    cache->function_types[cache->function_type_count - 1] = function_type;
    return function_type;
}

llir_type_t *llir_type_cache_get_enumeration(llir_type_cache_t *cache, size_t bit_size, size_t member_count) {
    for(size_t i = 0; i < cache->type_count; i++) {
        if(cache->types[i]->kind != LLIR_TYPE_KIND_ENUMERATION) continue;
        if(cache->types[i]->enumeration.bit_size != bit_size) continue;
        if(cache->types[i]->enumeration.member_count != member_count) continue;
        return cache->types[i];
    }
    llir_type_t *type = make_type(LLIR_TYPE_KIND_ENUMERATION);
    type->enumeration.bit_size = bit_size;
    type->enumeration.member_count = member_count;
    cache_add(cache, type);
    return type;
}