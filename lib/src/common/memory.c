#include "charon/memory.h"

#include "common/fatal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct charon_memory_allocator {
    size_t entry_count;
    void **entries;
};

static size_t find_index(charon_memory_allocator_t *allocator, void *ptr) {
    for(size_t i = 0; i < allocator->entry_count; i++) {
        if(allocator->entries[i] != ptr) continue;
        return i;
    }
    fatal("pointer `%p` not managed by allocator", ptr);
}

charon_memory_allocator_t *charon_memory_allocator_make() {
    charon_memory_allocator_t *allocator = malloc(sizeof(charon_memory_allocator_t));
    allocator->entry_count = 0;
    allocator->entries = NULL;
    return allocator;
}

void charon_memory_allocator_free(charon_memory_allocator_t *allocator) {
    for(size_t i = 0; i < allocator->entry_count; i++) free(allocator->entries[i]);
    free(allocator->entries);
    free(allocator);
}

void *charon_memory_register_ptr(charon_memory_allocator_t *allocator, void *ptr) {
    for(size_t i = 0; i < allocator->entry_count; i++) {
        if(allocator->entries[i] != ptr) continue;
        fatal("double register of pointer `%p`", ptr);
        assert(false);
    }

    allocator->entries = reallocarray(allocator->entries, ++allocator->entry_count, sizeof(void *));
    allocator->entries[allocator->entry_count - 1] = ptr;
    return ptr;
}

void *charon_memory_allocate(charon_memory_allocator_t *allocator, size_t size) {
    void *ptr = malloc(size);
    charon_memory_register_ptr(allocator, ptr);
    return ptr;
}

void *charon_memory_allocate_resize(charon_memory_allocator_t *allocator, void *ptr, size_t size) {
    if(ptr == NULL) return charon_memory_allocate(allocator, size);
    size_t index = find_index(allocator, ptr);
    allocator->entries[index] = realloc(ptr, size);
    return allocator->entries[index];
}

void *charon_memory_allocate_array(charon_memory_allocator_t *allocator, void *array, size_t element_count, size_t element_size) {
    return charon_memory_allocate_resize(allocator, array, element_count * element_size);
}

void charon_memory_free(charon_memory_allocator_t *allocator, void *ptr) {
    if(ptr == NULL) return;
    size_t index = find_index(allocator, ptr);
    memmove(&allocator->entries[index], &allocator->entries[index + 1], (allocator->entry_count - index - 1) * sizeof(void *));
    allocator->entries = reallocarray(allocator->entries, --allocator->entry_count, sizeof(void *));
    free(ptr);
}
