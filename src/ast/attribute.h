#pragma once

#include "lib/source.h"

#include <stdint.h>
#include <stddef.h>

#define AST_ATTRIBUTE_LIST_INIT ((ast_attribute_list_t) { .attribute_count = 0, .attributes = NULL })

typedef enum {
    AST_ATTRIBUTE_ARGUMENT_TYPE_STRING,
    AST_ATTRIBUTE_ARGUMENT_TYPE_NUMBER
} ast_attribute_argument_type_t;

typedef struct {
    ast_attribute_argument_type_t type;
    union {
        const char *string;
        uintmax_t number;
    } value;
} ast_attribute_argument_t;

typedef struct {
    const char *kind;
    size_t argument_count;
    ast_attribute_argument_t *arguments;
    source_location_t source_location;
    bool consumed;
} ast_attribute_t;

typedef struct {
    ast_attribute_t *attributes;
    size_t attribute_count;
} ast_attribute_list_t;

void ast_attribute_add(ast_attribute_list_t *list, const char *kind, ast_attribute_argument_t *arguments, size_t argument_count, source_location_t source_location);
ast_attribute_t *ast_attribute_find(ast_attribute_list_t *list, const char *kind, ast_attribute_argument_type_t *argument_types, size_t argument_type_count);