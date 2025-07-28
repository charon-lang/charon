#include "type.h"

#include "lib/alloc.h"

#define DEFINE_GET(NAME, TYPE)                                         \
    ast_type_t *ast_type_get_##NAME(ast_attribute_list_t attributes) { \
        static ast_type_t *type = NULL;                                \
        if(type == NULL) type = TYPE;                                  \
        return type;                                                   \
    }

#define DEFINE_GET_ALIAS(NAME, TO)                                     \
    ast_type_t *ast_type_get_##NAME(ast_attribute_list_t attributes) { \
        return ast_type_get_##TO(attributes);                          \
    }

static ast_type_t *make_type(ast_type_kind_t kind, ast_attribute_list_t attributes) {
    ast_type_t *type = alloc(sizeof(ast_type_t));
    type->attributes = attributes;
    type->is_reference = false;
    type->kind = kind;
    return type;
}

ast_type_t *ast_type_integer_make(size_t bit_size, bool is_signed, ast_attribute_list_t attributes) {
    ast_type_t *type = make_type(AST_TYPE_KIND_INTEGER, attributes);
    type->integer.bit_size = bit_size, type->integer.is_signed = is_signed;
    return type;
}

ast_type_t *ast_type_reference_make(const char *type_name, size_t module_count, const char **modules, size_t generic_parameter_count, ast_type_t **generic_parameters, ast_attribute_list_t attributes) {
    ast_type_t *type = alloc(sizeof(ast_type_t));
    type->is_reference = true;
    type->reference.module_count = module_count;
    type->reference.modules = modules;
    type->reference.type_name = type_name;
    type->reference.generic_parameter_count = generic_parameter_count;
    type->reference.generic_parameters = generic_parameters;
    return type;
}

ast_type_t *ast_type_void_make(ast_attribute_list_t attributes) {
    return make_type(AST_TYPE_KIND_VOID, attributes);
}

ast_type_t *ast_type_pointer_make(ast_type_t *pointee, ast_attribute_list_t attributes) {
    ast_type_t *type = make_type(AST_TYPE_KIND_POINTER, attributes);
    type->pointer.pointee = pointee;
    return type;
}

ast_type_t *ast_type_tuple_make(size_t type_count, ast_type_t **types, ast_attribute_list_t attributes) {
    ast_type_t *type = make_type(AST_TYPE_KIND_TUPLE, attributes);
    type->tuple.type_count = type_count;
    type->tuple.types = types;
    return type;
}

ast_type_t *ast_type_array_make(ast_type_t *type, size_t size, ast_attribute_list_t attributes) {
    ast_type_t *new_type = make_type(AST_TYPE_KIND_ARRAY, attributes);
    new_type->array.type = type;
    new_type->array.size = size;
    return new_type;
}

ast_type_t *ast_type_structure_make(size_t member_count, ast_type_structure_member_t *members, ast_attribute_list_t attributes) {
    ast_type_t *type = make_type(AST_TYPE_KIND_STRUCTURE, attributes);
    type->structure.member_count = member_count;
    type->structure.members = members;
    return type;
}

ast_type_t *ast_type_function_reference_make(ast_type_function_t *function_type, ast_attribute_list_t attributes) {
    ast_type_t *type = make_type(AST_TYPE_KIND_FUNCTION_REFERENCE, attributes);
    type->function_reference.function_type = function_type;
    return type;
}

ast_type_function_t *ast_type_function_make(size_t argument_count, ast_type_t **arguments, bool varargs, ast_type_t *return_type) {
    ast_type_function_t *function_type = alloc(sizeof(ast_type_function_t));
    function_type->return_type = return_type;
    function_type->argument_count = argument_count;
    function_type->arguments = arguments;
    function_type->varargs = varargs;
    return function_type;
}
