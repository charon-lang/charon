#pragma once

#include <stddef.h>

typedef struct allocator allocator_t;

allocator_t *allocator_make();
void allocator_free(allocator_t *allocator);

void allocator_set_active(allocator_t *allocator);

void *alloc(size_t size);
void *alloc_resize(void *ptr, size_t size);
void *alloc_array(void *array, size_t element_count, size_t element_size);
char *alloc_strdup(const char *str);
void alloc_free(void *ptr);