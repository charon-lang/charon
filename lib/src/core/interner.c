#include "interner.h"

#include "core/store.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINIMUM_CAPACITY 256
#define MAXIMUM_USAGE 0.75

typedef struct {
    store_handle_t handle;
    uint64_t value_hash;
} entry_t;

struct interner {
    store_t *store;
    entry_t *entries;
    size_t count;
    size_t capacity;
    size_t value_size;
    interner_size_fn_t size_fn;
    interner_hash_fn_t hash_fn;
    interner_equality_fn_t equality_fn;
    store_drop_fn_t drop_fn;
};

static size_t interner_value_size(interner_t *interner, const void *value) {
    if(interner->size_fn == nullptr) return interner->value_size;
    return interner->size_fn(interner->store, value);
}

static entry_t *find_entry(store_t *store, entry_t *entries, size_t capacity, const void *value, size_t value_size, uint64_t value_hash, interner_equality_fn_t equality_fn) {
    size_t mask = capacity - 1;
    size_t index = (size_t) (value_hash & mask);
    while(true) {
        entry_t *entry = &entries[index];
        if(store_handle_is_dead(store, entry->handle)) return entry;
        if(entry->value_hash == value_hash && equality_fn(store, store_get(store, entry->handle), value, value_size)) return entry;
        index = (index + 1) & mask;
    }
}

static void resize_interner(interner_t *interner, size_t new_capacity) {
    entry_t *entries = calloc(new_capacity, sizeof(entry_t));
    for(size_t i = 0; i < interner->capacity; i++) {
        if(store_handle_is_dead(interner->store, interner->entries[i].handle)) continue;

        void *value = store_get(interner->store, interner->entries[i].handle);
        entry_t *new_entry = find_entry(interner->store, entries, new_capacity, value, interner_value_size(interner, value), interner->entries[i].value_hash, interner->equality_fn);
        new_entry->handle = interner->entries[i].handle;
        new_entry->value_hash = interner->entries[i].value_hash;
    }
    free(interner->entries);
    interner->entries = entries;
    interner->capacity = new_capacity;
}


interner_t *interner_new(size_t initial_capacity, size_t value_size, interner_size_fn_t size_fn, interner_hash_fn_t hash_fn, interner_equality_fn_t equality_fn, store_drop_fn_t drop_fn) {
    if(initial_capacity < MINIMUM_CAPACITY) initial_capacity = MINIMUM_CAPACITY;

    size_t capacity = 1;
    while(capacity < initial_capacity) capacity <<= 1;

    assert(size_fn != nullptr || value_size != 0);

    interner_t *interner = malloc(sizeof(interner_t));
    interner->entries = calloc(capacity, sizeof(entry_t));
    interner->capacity = capacity;
    interner->count = 0;
    interner->value_size = value_size;
    interner->size_fn = size_fn;
    interner->hash_fn = hash_fn;
    interner->equality_fn = equality_fn;
    interner->drop_fn = drop_fn;
    return interner;
}

void interner_destroy(interner_t *interner) {
    // TODO: possibly force free handles
    // for(size_t i = 0; i < interner->capacity; i++) free(interner->entries[i].value);
    free(interner->entries);
    free(interner);
}

store_handle_t interner_intern(interner_t *interner, const void *value) {
    if((double) interner->count / (double) interner->capacity > MAXIMUM_USAGE) resize_interner(interner, interner->capacity * 2);

    size_t value_size = interner_value_size(interner, value);

    uint64_t hash = interner->hash_fn(interner->store, value, value_size);
    entry_t *entry = find_entry(interner->store, interner->entries, interner->capacity, value, value_size, hash, interner->equality_fn);
    if(!store_handle_is_dead(interner->store, entry->handle)) return entry->handle;

    void *copy = malloc(value_size);
    memcpy(copy, value, value_size);

    entry->handle = store_insert(interner->store, copy, interner->drop_fn);
    entry->value_hash = hash;

    interner->count++;
    return entry->handle;
}

// void interner_free(interner_t *interner, void *value) {
//     size_t value_size = interner_value_size(interner, value);

//     uint64_t hash = interner->hash_fn(value, value_size);
//     entry_t *entry = find_entry(interner->entries, interner->capacity, value, value_size, hash, interner->equality_fn);
//     if(entry->value == nullptr) return;

//     free(entry->value);
//     entry->value = nullptr;
//     entry->value_hash = 0;
// }

size_t interner_count(interner_t *interner) {
    return interner->count;
}
