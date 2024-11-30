#pragma once

#include "lib/source.h"

#include <stdint.h>
#include <stddef.h>

#define HLIR_ATTRIBUTE_LIST_INIT ((hlir_attribute_list_t) { .attribute_count = 0, .attributes = NULL })

typedef enum {
    HLIR_ATTRIBUTE_ARGUMENT_TYPE_STRING,
    HLIR_ATTRIBUTE_ARGUMENT_TYPE_NUMBER
} hlir_attribute_argument_type_t;

typedef struct {
    hlir_attribute_argument_type_t type;
    union {
        const char *string;
        uintmax_t number;
    } value;
} hlir_attribute_argument_t;

typedef struct {
    const char *kind;
    size_t argument_count;
    hlir_attribute_argument_t *arguments;
    source_location_t source_location;
    bool consumed;
} hlir_attribute_t;

typedef struct {
    hlir_attribute_t *attributes;
    size_t attribute_count;
} hlir_attribute_list_t;

void hlir_attribute_add(hlir_attribute_list_t *list, const char *kind, hlir_attribute_argument_t *arguments, size_t argument_count, source_location_t source_location);
hlir_attribute_t *hlir_attribute_find(hlir_attribute_list_t *list, const char *kind, hlir_attribute_argument_type_t *argument_types, size_t argument_type_count);