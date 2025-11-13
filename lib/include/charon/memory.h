#pragma once

#include <stddef.h>

typedef struct charon_memory_allocator charon_memory_allocator_t;

charon_memory_allocator_t *charon_memory_allocator_make();
void charon_memory_allocator_free(charon_memory_allocator_t *allocator);

void *charon_memory_register_ptr(charon_memory_allocator_t *allocator, void *ptr);
void *charon_memory_allocate(charon_memory_allocator_t *allocator, size_t size);
void *charon_memory_allocate_resize(charon_memory_allocator_t *allocator, void *ptr, size_t size);
void *charon_memory_allocate_array(charon_memory_allocator_t *allocator, void *array, size_t element_count, size_t element_size);
void charon_memory_free(charon_memory_allocator_t *allocator, void *ptr);
