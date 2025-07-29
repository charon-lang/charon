#pragma once

#include "lib/memory.h"

#include <assert.h>
#include <string.h>

static inline void *alloc(size_t size) {
    memory_allocator_t *allocator = memory_active_allocator_get();
    assert(allocator != NULL);
    return memory_allocate(allocator, size);
}

static inline void *alloc_resize(void *ptr, size_t size) {
    memory_allocator_t *allocator = memory_active_allocator_get();
    assert(allocator != NULL);
    return memory_allocate_resize(allocator, ptr, size);
}

static inline void *alloc_array(void *array, size_t element_count, size_t element_size) {
    memory_allocator_t *allocator = memory_active_allocator_get();
    assert(allocator != NULL);
    return memory_allocate_array(allocator, array, element_count, element_size);
}

static inline char *alloc_strdup(const char *str) {
    memory_allocator_t *allocator = memory_active_allocator_get();
    assert(allocator != NULL);
    char *ptr = strdup(str);
    memory_register_ptr(allocator, ptr);
    return ptr;
}

static inline void alloc_free(void *ptr) {
    memory_allocator_t *allocator = memory_active_allocator_get();
    assert(allocator != NULL);
    memory_free(allocator, ptr);
}
