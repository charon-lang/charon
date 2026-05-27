#include "store.h"

#include "stdlib.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define INITIAL_CAPACITY 256

typedef struct {
    uint32_t revision;
    uint32_t counter;
    void *data;
    store_drop_fn_t drop_fn;
} store_entry_t;

struct store {
    size_t capacity;
    size_t free_entries;
    store_entry_t *entries;
    void *free_entry_list;
};

static void store_expand(store_t *store, size_t new_size) {
    store->entries = reallocarray(store->entries, new_size, sizeof(store_entry_t));

    for(size_t i = store->capacity; i < new_size; i++) {
        store->entries[i].revision = 1;
        store->entries[i].data = store->free_entry_list;
        store->free_entry_list = &store->entries[i];
    }

    store->free_entries += new_size - store->capacity;
    store->capacity = new_size;
}

static store_entry_t *store_lookup(store_t *store, store_handle_t handle) {
    assert(handle.index < store->capacity);
    store_entry_t *entry = &store->entries[handle.index];
    assert(entry->revision == handle.revision);
    return entry;
}

bool store_handle_is_dead(store_t *store, store_handle_t handle) {
    assert(handle.index < store->capacity);
    store_entry_t *entry = &store->entries[handle.index];
    assert(entry->revision >= handle.revision);
    return entry->revision != handle.revision;
}

store_t *store_new() {
    store_t *store = malloc(sizeof(store_t));
    store->capacity = 0;
    store->free_entries = 0;
    store->entries = nullptr;
    store->free_entry_list = nullptr;
    store_expand(store, INITIAL_CAPACITY);
    return store;
}

void store_destroy(store_t *store) {
    free(store->entries);
    free(store);
}

store_handle_t store_insert(store_t *store, void *data, store_drop_fn_t drop_fn) {
    if(store->free_entries == 0) store_expand(store, store->capacity * 2);

    store_entry_t *entry = store->free_entry_list;
    store->free_entry_list = entry->data;

    entry->data = data;
    entry->drop_fn = drop_fn;

    assert(entry->counter == 0);
    entry->counter = 1;

    size_t index = ((uintptr_t) entry - (uintptr_t) store->entries) / sizeof(store_entry_t);

    return (store_handle_t) { .index = index, .revision = entry->revision };
}

void *store_get(store_t *store, store_handle_t handle) {
    return store_lookup(store, handle)->data;
}

void store_ref(store_t *store, store_handle_t handle) {
    store_entry_t *entry = store_lookup(store, handle);
    assert(entry->counter < UINT32_MAX);
    entry->counter++;
}

void store_unref(store_t *store, store_handle_t handle) {
    store_entry_t *entry = store_lookup(store, handle);
    assert(entry->counter > 0);
    entry->counter--;
    if(entry->counter == 0) {
        entry->drop_fn(entry->data);
        assert(entry->revision < UINT32_MAX);
        entry->revision++;
        entry->data = store->free_entry_list;
        store->free_entry_list = entry;
    }
}
