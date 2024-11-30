#include "attribute.h"

#include "lib/alloc.h"

#include <string.h>

hlir_attribute_t *hlir_attribute_find(hlir_attribute_list_t *list, const char *kind, hlir_attribute_argument_type_t *argument_types, size_t argument_type_count) {
    for(size_t i = 0; i < list->attribute_count; i++) {
        if(list->attributes[i].consumed) continue;
        if(strcasecmp(list->attributes[i].kind, kind) != 0) continue;
        if(list->attributes[i].argument_count != argument_type_count) continue;
        for(size_t j = 0; j < argument_type_count; j++) if(list->attributes[i].arguments[j].type != argument_types[j]) goto cont;
        list->attributes[i].consumed = true;
        return &list->attributes[i];
        cont:
    }
    return NULL;
}

void hlir_attribute_add(hlir_attribute_list_t *list, const char *kind, hlir_attribute_argument_t *arguments, size_t argument_count, source_location_t source_location) {
    list->attributes = alloc_array(list->attributes, ++list->attribute_count, sizeof(hlir_attribute_t));
    list->attributes[list->attribute_count - 1] = (hlir_attribute_t) { .kind = kind, .arguments = arguments, .argument_count = argument_count, .source_location = source_location, .consumed = false };
}