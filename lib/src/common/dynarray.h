#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define DYNARRAY_INIT { .element_count = 0, .elements = nullptr }

#define DYNARRAY(TYPE)        \
    struct {                  \
        size_t element_count; \
        TYPE *elements;       \
    }

#define DYNARRAY_PUSH(ARRAY, ELEMENT)                                                                                \
    {                                                                                                                \
        (ARRAY)->elements = reallocarray((ARRAY)->elements, ++(ARRAY)->element_count, sizeof((ARRAY)->elements[0])); \
        (ARRAY)->elements[(ARRAY)->element_count - 1] = ELEMENT;                                                     \
    }

#define DYNARRAY_POP(ARRAY)                                                                                          \
    ({                                                                                                               \
        assert((ARRAY)->element_count > 0);                                                                          \
        size_t index = (ARRAY)->elements[(ARRAY)->element_count - 1];                                                \
        (ARRAY)->elements = reallocarray((ARRAY)->elements, --(ARRAY)->element_count, sizeof((ARRAY)->elements[0])); \
        index;                                                                                                       \
    })

#define DYNARRAY_CLEAR(ARRAY)        \
    {                                \
        free((ARRAY)->elements);     \
        (ARRAY)->elements = nullptr; \
        (ARRAY)->element_count = 0;  \
    }

#define DYNARRAY_GET(ARRAY, INDEX)                \
    {                                             \
        assert((INDEX) < (ARRAY)->element_count); \
        (ARRAY)->elements[(INDEX)];               \
    }
