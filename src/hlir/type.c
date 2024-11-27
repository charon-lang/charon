#include "type.h"

#include "lib/alloc.h"

#define DEFINE_GET(NAME, TYPE) \
    hlir_type_t *hlir_type_get_##NAME(hlir_attribute_list_t attributes) { \
        static hlir_type_t *type = NULL; \
        if(type == NULL) type = TYPE; \
        return type; \
    }

#define DEFINE_GET_ALIAS(NAME, TO) \
    hlir_type_t *hlir_type_get_##NAME(hlir_attribute_list_t attributes) { \
        return hlir_type_get_##TO(attributes); \
    }

static hlir_type_t *make_type(hlir_type_kind_t kind, hlir_attribute_list_t attributes) {
    hlir_type_t *type = alloc(sizeof(hlir_type_t));
    type->attributes = attributes;
    type->is_reference = false;
    type->kind = kind;
    return type;
}

hlir_type_t *hlir_type_integer_make(size_t bit_size, bool is_signed, hlir_attribute_list_t attributes) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_INTEGER, attributes);
    type->integer.bit_size = bit_size,
    type->integer.is_signed = is_signed;
    return type;
}

hlir_type_t *hlir_type_reference_make(const char *type_name, size_t module_count, const char **modules, hlir_attribute_list_t attributes) {
    hlir_type_t *type = alloc(sizeof(hlir_type_t));
    type->is_reference = true;
    type->reference.module_count = module_count;
    type->reference.modules = modules;
    type->reference.type_name = type_name;
    return type;
}

hlir_type_t *hlir_type_void_make(hlir_attribute_list_t attributes) {
    return make_type(HLIR_TYPE_KIND_VOID, attributes);
}

hlir_type_t *hlir_type_pointer_make(hlir_type_t *pointee, hlir_attribute_list_t attributes) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_POINTER, attributes);
    type->pointer.pointee = pointee;
    return type;
}

hlir_type_t *hlir_type_tuple_make(size_t type_count, hlir_type_t **types, hlir_attribute_list_t attributes) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_TUPLE, attributes);
    type->tuple.type_count = type_count;
    type->tuple.types = types;
    return type;
}

hlir_type_t *hlir_type_array_make(hlir_type_t *type, size_t size, hlir_attribute_list_t attributes) {
    hlir_type_t *new_type = make_type(HLIR_TYPE_KIND_ARRAY, attributes);
    new_type->array.type = type;
    new_type->array.size = size;
    return new_type;
}

hlir_type_t *hlir_type_structure_make(size_t member_count, hlir_type_structure_member_t *members, hlir_attribute_list_t attributes) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_STRUCTURE, attributes);
    type->structure.member_count = member_count;
    type->structure.members = members;
    return type;
}

hlir_type_t *hlir_type_function_reference_make(hlir_type_function_t *function_type, hlir_attribute_list_t attributes) {
    hlir_type_t *type = make_type(HLIR_TYPE_KIND_FUNCTION_REFERENCE, attributes);
    type->function_reference.function_type = function_type;
    return type;
}

hlir_type_function_t *hlir_type_function_make(size_t argument_count, hlir_type_t **arguments, bool varargs, hlir_type_t *return_type) {
    hlir_type_function_t *function_type = alloc(sizeof(hlir_type_function_t));
    function_type->return_type = return_type;
    function_type->argument_count = argument_count;
    function_type->arguments = arguments;
    function_type->varargs = varargs;
    return function_type;
}