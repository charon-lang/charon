#pragma once

#include "charon/element.h"

#include <stddef.h>
#include <stdlib.h>

#define COLLECTOR_INIT ((collector_t) { .count = 0, .elements = nullptr })

typedef struct {
    size_t count;
    const charon_element_inner_t **elements;
} collector_t;

static inline void collector_push(collector_t *collector, const charon_element_inner_t *element) {
    collector->elements = reallocarray(collector->elements, ++collector->count, sizeof(charon_element_inner_t *));
    collector->elements[collector->count - 1] = element;
}

static inline void collector_clear(collector_t *collector) {
    free(collector->elements);
    *collector = COLLECTOR_INIT;
}
