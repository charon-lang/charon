#include "attribute.h"

#include <stdlib.h>

static void list_add(hlir_attribute_list_t *list, hlir_attribute_kind_t kind, source_location_t source_location) {
    list->attributes = reallocarray(list->attributes, ++list->attribute_count, sizeof(hlir_attribute_t));
    list->attributes[list->attribute_count - 1] = (hlir_attribute_t) { .kind = kind, .source_location = source_location, .consumed = false };
}

hlir_attribute_t *hlir_attribute_find(hlir_attribute_list_t *list, hlir_attribute_kind_t kind) {
    for(size_t i = 0; i < list->attribute_count; i++) {
        if(list->attributes[i].consumed || list->attributes[i].kind != kind) continue;
        return &list->attributes[i];
    }
    return NULL;
}

void hlir_attribute_list_add_packed(hlir_attribute_list_t *list, source_location_t source_location) {
    list_add(list, HLIR_ATTRIBUTE_KIND_PACKED, source_location);
}