#pragma once

#include "ast/attribute.h"

#include <stddef.h>

typedef enum {
    AST_TYPE_KIND_VOID,
    AST_TYPE_KIND_INTEGER,
    AST_TYPE_KIND_POINTER,
    AST_TYPE_KIND_TUPLE,
    AST_TYPE_KIND_ARRAY,
    AST_TYPE_KIND_STRUCTURE,
    AST_TYPE_KIND_FUNCTION_REFERENCE
} ast_type_kind_t;

typedef struct ast_type ast_type_t;
typedef struct ast_type_function ast_type_function_t;

typedef struct {
    ast_type_t *type;
    const char *name;
} ast_type_structure_member_t;

struct ast_type {
    bool is_reference;
    ast_attribute_list_t attributes;
    union {
        struct {
            ast_type_kind_t kind;
            union {
                struct {
                    bool is_signed;
                    size_t bit_size;
                } integer;
                struct {
                    ast_type_t *pointee;
                } pointer;
                struct {
                    size_t type_count;
                    ast_type_t **types;
                } tuple;
                struct {
                    ast_type_t *type;
                    size_t size;
                } array;
                struct {
                    size_t member_count;
                    ast_type_structure_member_t *members;
                } structure;
                struct {
                    ast_type_function_t *function_type;
                } function_reference;
            };
        };
        struct {
            const char **modules;
            size_t module_count;
            const char *type_name;
            size_t generic_parameter_count;
            ast_type_t **generic_parameters;
        } reference;
    };
};

struct ast_type_function {
    ast_type_t *return_type;
    size_t argument_count;
    ast_type_t **arguments;
    bool varargs;
};

ast_type_t *ast_type_reference_make(const char *type_name, size_t module_count, const char **modules, size_t generic_parameter_count, ast_type_t **generic_parameters, ast_attribute_list_t attributes);

ast_type_t *ast_type_void_make(ast_attribute_list_t attributes);
ast_type_t *ast_type_integer_make(size_t bit_size, bool is_signed, ast_attribute_list_t attributes);
ast_type_t *ast_type_pointer_make(ast_type_t *pointee, ast_attribute_list_t attributes);
ast_type_t *ast_type_tuple_make(size_t type_count, ast_type_t **types, ast_attribute_list_t attributes);
ast_type_t *ast_type_array_make(ast_type_t *type, size_t size, ast_attribute_list_t attributes);
ast_type_t *ast_type_structure_make(size_t member_count, ast_type_structure_member_t *members, ast_attribute_list_t attributes);
ast_type_t *ast_type_function_reference_make(ast_type_function_t *function_type, ast_attribute_list_t attributes);

ast_type_function_t *ast_type_function_make(size_t argument_count, ast_type_t **arguments, bool varargs, ast_type_t *return_type);
