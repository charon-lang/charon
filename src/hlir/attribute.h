#pragma once

#include "lib/source.h"

#include <stddef.h>

#define HLIR_ATTRIBUTE_LIST_INIT ((hlir_attribute_list_t) { .attribute_count = 0, .attributes = NULL })

typedef enum {
    HLIR_ATTRIBUTE_KIND_PACKED,
    HLIR_ATTRIBUTE_KIND_LINK
} hlir_attribute_kind_t;

typedef struct {
    hlir_attribute_kind_t kind;
    source_location_t source_location;
    bool consumed;
    union {
        struct {
            const char *name;
        } link;
    };
} hlir_attribute_t;

typedef struct {
    hlir_attribute_t *attributes;
    size_t attribute_count;
} hlir_attribute_list_t;

hlir_attribute_t *hlir_attribute_find(hlir_attribute_list_t *list, hlir_attribute_kind_t kind);

void hlir_attribute_list_add_packed(hlir_attribute_list_t *list, source_location_t source_location);
void hlir_attribute_list_add_link(hlir_attribute_list_t *list, const char *name, source_location_t source_location);