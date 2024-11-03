#include "type.h"

#include <stdlib.h>

#define DEFINE_GET(NAME, TYPE) \
    hlir_type_t *hlir_type_get_##NAME() { \
        static hlir_type_t *type = NULL; \
        if(type == NULL) type = TYPE; \
        return type; \
    }

#define DEFINE_GET_ALIAS(NAME, TO) \
    hlir_type_t *hlir_type_get_##NAME() { \
        return hlir_type_get_##TO(); \
    }

static hlir_type_t *make_type(hlir_type_kind_t kind) {
    hlir_type_t *type = malloc(sizeof(hlir_type_t));
    type->is_reference = false;
    type->kind = kind;
    return type;
}

static hlir_type_t *make_int_type(size_t bit_size, bool is_signed) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_INTEGER);
    type->integer.bit_size = bit_size,
    type->integer.is_signed = is_signed;
    return type;
}

hlir_type_t *hlir_type_reference_make(const char *type_name) {
    hlir_type_t *type = malloc(sizeof(hlir_type_t));
    type->is_reference = true;
    type->reference.type_name = type_name;
    return type;
}

DEFINE_GET(void, make_type(HLIR_TYPE_KIND_VOID));
DEFINE_GET(bool, make_int_type(1, false));
DEFINE_GET_ALIAS(char, u8);
DEFINE_GET_ALIAS(ptr, u64);

DEFINE_GET_ALIAS(uint, u64);
DEFINE_GET(u64, make_int_type(64, false))
DEFINE_GET(u32, make_int_type(32, false))
DEFINE_GET(u16, make_int_type(16, false))
DEFINE_GET(u8, make_int_type(8, false))

DEFINE_GET_ALIAS(int, i64);
DEFINE_GET(i64, make_int_type(64, true))
DEFINE_GET(i32, make_int_type(32, true))
DEFINE_GET(i16, make_int_type(16, true))
DEFINE_GET(i8, make_int_type(8, true))

hlir_type_t *hlir_type_pointer_make(hlir_type_t *pointee) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_POINTER);
    type->pointer.pointee = pointee;
    return type;
}

hlir_type_t *hlir_type_tuple_make(size_t type_count, hlir_type_t **types) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_TUPLE);
    type->tuple.type_count = type_count;
    type->tuple.types = types;
    return type;
}

hlir_type_t *hlir_type_array_make(hlir_type_t *type, size_t size) {
    hlir_type_t *new_type = make_type(HLIR_TYPE_KIND_ARRAY);
    new_type->array.type = type;
    new_type->array.size = size;
    return new_type;
}

hlir_type_t *hlir_type_structure_make(size_t member_count, hlir_type_t **members) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_STRUCTURE);
    type->structure.member_count = member_count;
    type->structure.members = members;
    return type;
}

hlir_type_t *hlir_type_function_make(size_t argument_count, hlir_type_t **arguments, bool varargs, hlir_type_t *return_type) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_FUNCTION);
    type->function.return_type = return_type;
    type->function.argument_count = argument_count;
    type->function.arguments = arguments;
    type->function.varargs = varargs;
    return type;
}