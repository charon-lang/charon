#pragma once

#include "lib/memory.h"

#include <string.h>

static inline void *alloc(size_t size) {
    return memory_allocate(memory_active_allocator_get(), size);
}

static inline void *alloc_resize(void *ptr, size_t size) {
    return memory_allocate_resize(memory_active_allocator_get(), ptr, size);
}

static inline void *alloc_array(void *array, size_t element_count, size_t element_size) {
    return memory_allocate_array(memory_active_allocator_get(), array, element_count, element_size);
}

static inline char *alloc_strdup(const char *str) {
    char *ptr = strdup(str);
    memory_register_ptr(memory_active_allocator_get(), ptr);
    return ptr;
}

static inline void alloc_free(void *ptr) {
    memory_free(memory_active_allocator_get(), ptr);
}
