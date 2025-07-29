#pragma once

#include "lib/alloc.h"

#include <stddef.h>

#define VECTOR_DEFINE(NAME, TYPE) VECTOR_DEFINE_EXT(NAME, TYPE, alloc_array)

#define VECTOR_DEFINE_EXT(NAME, TYPE, ALLOCATOR)                                       \
    typedef struct {                                                                   \
        size_t size;                                                                   \
        TYPE *values;                                                                  \
    } vector_##NAME##_t;                                                               \
                                                                                       \
    static inline vector_##NAME##_t vector_##NAME##_init() {                           \
        return (vector_##NAME##_t) { .size = 0, .values = NULL };                      \
    }                                                                                  \
                                                                                       \
    static inline TYPE vector_##NAME##_append(vector_##NAME##_t *vector, TYPE value) { \
        vector->values = ALLOCATOR(vector->values, ++vector->size, sizeof(TYPE));      \
        vector->values[vector->size - 1] = value;                                      \
        return vector->values[vector->size - 1];                                       \
    }

#define VECTOR_SIZE(VECTOR) ((VECTOR)->size)

#define VECTOR_FOREACH(VECTOR, ITERATOR, ELEMENT)                                                                      \
    for(size_t __keep = true, ITERATOR = 0; __keep && ITERATOR < (VECTOR)->size; __keep = !__keep, ITERATOR++)         \
        for([[maybe_unused]] typeof(*(VECTOR)->values) ELEMENT = (VECTOR)->values[ITERATOR]; __keep; __keep = !__keep)

#define VECTOR_FOREACH_ELEMENT_REF(VECTOR, ITERATOR) (&(VECTOR)->values[ITERATOR])
