#pragma once

#include <stddef.h>

typedef struct memory_allocator memory_allocator_t;

memory_allocator_t *memory_allocator_make();
void memory_allocator_free(memory_allocator_t *allocator);

void memory_active_allocator_set(memory_allocator_t *allocator);
memory_allocator_t *memory_active_allocator_get();

void *memory_register_ptr(memory_allocator_t *allocator, void *ptr);
void *memory_allocate(memory_allocator_t *allocator, size_t size);
void *memory_allocate_resize(memory_allocator_t *allocator, void *ptr, size_t size);
void *memory_allocate_array(memory_allocator_t *allocator, void *array, size_t element_count, size_t element_size);
void memory_free(memory_allocator_t *allocator, void *ptr);