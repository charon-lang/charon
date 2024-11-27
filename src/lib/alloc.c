#include "alloc.h"

#include "lib/log.h"

#include <stdlib.h>
#include <string.h>

static __thread struct allocator *g_current_allocator = NULL;

struct allocator {
    size_t entry_count;
    void **entries;
};

static size_t find_index(allocator_t *allocator, void *ptr) {
    for(size_t i = 0; i < allocator->entry_count; i++) {
        if(allocator->entries[i] != ptr) continue;
        return i;
    }
    log_fatal("alloc find index failed");
}

allocator_t *allocator_make() {
    allocator_t *allocator = malloc(sizeof(allocator_t));
    allocator->entry_count = 0;
    allocator->entries = NULL;
    return allocator;
}

void allocator_free(allocator_t *allocator) {
    for(size_t i = 0; i < allocator->entry_count; i++) free(allocator->entries[i]);
    free(allocator->entries);
    free(allocator);
}

void allocator_set_active(allocator_t *allocator) {
    g_current_allocator = allocator;
}

void *alloc(size_t size) {
    if(g_current_allocator == NULL) log_fatal("allocation attempted with no active allocator");
    void *ptr = malloc(size);
    g_current_allocator->entries = reallocarray(g_current_allocator->entries, ++g_current_allocator->entry_count, sizeof(void *));
    g_current_allocator->entries[g_current_allocator->entry_count - 1] = ptr;
    return ptr;
}

void *alloc_resize(void *ptr, size_t size) {
    if(g_current_allocator == NULL) log_fatal("allocation resize attempted with no active allocator");
    if(ptr == NULL) return alloc(size);
    size_t index = find_index(g_current_allocator, ptr);
    g_current_allocator->entries[index] = realloc(ptr, size);
    return g_current_allocator->entries[index];
}

void *alloc_array(void *array, size_t element_count, size_t element_size) {
    return alloc_resize(array, element_count * element_size);
}

char *alloc_strdup(const char *str) {
    if(g_current_allocator == NULL) log_fatal("strdup attempted with no active allocator");
    char *ptr = strdup(str);
    g_current_allocator->entries = reallocarray(g_current_allocator->entries, ++g_current_allocator->entry_count, sizeof(void *));
    g_current_allocator->entries[g_current_allocator->entry_count - 1] = ptr;
    return ptr;
}

void alloc_free(void *ptr) {
    if(g_current_allocator == NULL) log_fatal("free attempted with no active allocator");
    if(ptr == NULL) return;
    size_t index = find_index(g_current_allocator, ptr);
    memmove(&g_current_allocator->entries[index], &g_current_allocator->entries[index + 1], (g_current_allocator->entry_count - index - 1) * sizeof(void *));
    g_current_allocator->entries = reallocarray(g_current_allocator->entries, --g_current_allocator->entry_count, sizeof(void *));
    free(ptr);
}