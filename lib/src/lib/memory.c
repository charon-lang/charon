#include "memory.h"

#include "lib/log.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct memory_allocator {
    size_t entry_count;
    void **entries;
};

static __thread memory_allocator_t *g_active_allocator = NULL;

static size_t find_index(memory_allocator_t *allocator, void *ptr) {
    for(size_t i = 0; i < allocator->entry_count; i++) {
        if(allocator->entries[i] != ptr) continue;
        return i;
    }
    log_fatal("pointer `%#lx` not managed by allocator", ptr);
}

memory_allocator_t *memory_allocator_make() {
    memory_allocator_t *allocator = malloc(sizeof(memory_allocator_t));
    allocator->entry_count = 0;
    allocator->entries = NULL;
    return allocator;
}

void memory_allocator_free(memory_allocator_t *allocator) {
    for(size_t i = 0; i < allocator->entry_count; i++) free(allocator->entries[i]);
    free(allocator->entries);
    free(allocator);
}

void memory_active_allocator_set(memory_allocator_t *allocator) {
    g_active_allocator = allocator;
}

memory_allocator_t *memory_active_allocator_get() {
    return g_active_allocator;
}

void *memory_register_ptr(memory_allocator_t *allocator, void *ptr) {
    for(size_t i = 0; i < allocator->entry_count; i++) {
        if(allocator->entries[i] != ptr) continue;
        log_fatal("double register of %p", ptr);
    }

    allocator->entries = reallocarray(allocator->entries, ++allocator->entry_count, sizeof(void *));
    allocator->entries[allocator->entry_count - 1] = ptr;
    return ptr;
}

void *memory_allocate(memory_allocator_t *allocator, size_t size) {
    void *ptr = malloc(size);
    memory_register_ptr(allocator, ptr);
    return ptr;
}

void *memory_allocate_resize(memory_allocator_t *allocator, void *ptr, size_t size) {
    if(ptr == NULL) return memory_allocate(allocator, size);
    size_t index = find_index(allocator, ptr);
    allocator->entries[index] = realloc(ptr, size);
    return allocator->entries[index];
}

void *memory_allocate_array(memory_allocator_t *allocator, void *array, size_t element_count, size_t element_size) {
    return memory_allocate_resize(allocator, array, element_count * element_size);
}

void memory_free(memory_allocator_t *allocator, void *ptr) {
    if(ptr == NULL) return;
    size_t index = find_index(allocator, ptr);
    memmove(&allocator->entries[index], &allocator->entries[index + 1], (allocator->entry_count - index - 1) * sizeof(void *));
    allocator->entries = reallocarray(allocator->entries, --allocator->entry_count, sizeof(void *));
    free(ptr);
}
