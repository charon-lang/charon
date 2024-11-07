#include "type.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static llir_type_t *make_type(llir_type_kind_t kind) {
    llir_type_t *type = malloc(sizeof(llir_type_t));
    type->kind = kind;
    return type;
}

bool llir_type_eq(llir_type_t *a, llir_type_t *b) {
    if(a->kind != b->kind) return false;
    switch(a->kind) {
        case LLIR_TYPE_KIND_VOID: return true;
        case LLIR_TYPE_KIND_INTEGER: return a->integer.bit_size == b->integer.bit_size && a->integer.is_signed == b->integer.is_signed;
        case LLIR_TYPE_KIND_POINTER: return llir_type_eq(a->pointer.pointee, b->pointer.pointee);
        case LLIR_TYPE_KIND_TUPLE:
            if(a->tuple.type_count != b->tuple.type_count) return false;
            for(size_t i = 0; i < a->tuple.type_count; i++) if(!llir_type_eq(a->tuple.types[i], b->tuple.types[i])) return false;
            return true;
        case LLIR_TYPE_KIND_ARRAY:
            if(a->array.size != b->array.size) return false;
            return llir_type_eq(a->array.type, b->array.type);
        case LLIR_TYPE_KIND_STRUCTURE:
            if(a->structure.member_count != b->structure.member_count) return false;
            for(size_t i = 0; i < a->structure.member_count; i++) {
                if(strcmp(a->structure.members[i].name, b->structure.members[i].name) != 0) return false;
                if(!llir_type_eq(a->structure.members[i].type, b->structure.members[i].type)) return false;
            }
            return true;
        case LLIR_TYPE_KIND_FUNCTION:
            if(a->function.varargs != b->function.varargs) return false;
            if(a->function.argument_count != b->function.argument_count) return false;
            for(size_t i = 0; i < a->function.argument_count; i++) if(!llir_type_eq(a->function.arguments[i], b->function.arguments[i])) return false;
            return llir_type_eq(a->function.return_type, b->function.return_type);
    }
    assert(false);
}

llir_type_t *llir_type_tuple_make(size_t type_count, llir_type_t **types) {
    llir_type_t *type = make_type(LLIR_TYPE_KIND_TUPLE);
    type->tuple.type_count = type_count;
    type->tuple.types = types;
    return type;
}