#include "interner.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MINIMUM_CAPACITY 256
#define MAXIMUM_USAGE 0.75

typedef struct {
    void *value;
    uint64_t value_hash;
} entry_t;

struct interner {
    entry_t *entries;
    size_t count;
    size_t capacity;
    size_t value_size;
    interner_hash_fn_t hash_fn;
    interner_equality_fn_t equality_fn;
};

static entry_t *find_entry(entry_t *entries, size_t capacity, const void *value, size_t value_size, uint64_t value_hash, interner_equality_fn_t equality_fn) {
    size_t mask = capacity - 1;
    size_t index = (size_t) (value_hash & mask);
    while(true) {
        entry_t *entry = &entries[index];
        if(entry->value == nullptr) return entry;
        if(entry->value_hash == value_hash && equality_fn(entry->value, value, value_size)) return entry;
        index = (index + 1) & mask;
    }
}

static void resize_interner(interner_t *interner, size_t new_capacity) {
    entry_t *entries = calloc(new_capacity, sizeof(entry_t));
    for(size_t i = 0; i < interner->capacity; i++) {
        if(interner->entries[i].value == nullptr) continue;
        entry_t *new_entry = find_entry(entries, new_capacity, interner->entries[i].value, interner->value_size, interner->entries[i].value_hash, interner->equality_fn);
        new_entry->value = interner->entries[i].value;
        new_entry->value_hash = interner->entries[i].value_hash;
    }
    free(interner->entries);
    interner->entries = entries;
    interner->capacity = new_capacity;
}

interner_t *interner_new(size_t initial_capacity, size_t value_size, interner_hash_fn_t hash_fn, interner_equality_fn_t equality_fn) {
    if(initial_capacity < MINIMUM_CAPACITY) initial_capacity = MINIMUM_CAPACITY;

    size_t capacity = 1;
    while(capacity < initial_capacity) capacity <<= 1;

    interner_t *interner = malloc(sizeof(interner_t));
    interner->entries = calloc(capacity, sizeof(entry_t));
    interner->count = 0;
    interner->value_size = value_size;
    interner->hash_fn = hash_fn;
    interner->equality_fn = equality_fn;
    return interner;
}

void interner_destroy(interner_t *interner) {
    for(size_t i = 0; i < interner->capacity; i++) free(interner->entries[i].value);
    free(interner->entries);
    free(interner);
}

const void *interner_intern(interner_t *interner, const void *value) {
    if((double) interner->count / (double) interner->capacity > MAXIMUM_USAGE) resize_interner(interner, interner->capacity * 2);

    uint64_t hash = interner->hash_fn(value, interner->value_size);
    entry_t *entry = find_entry(interner->entries, interner->capacity, value, interner->value_size, hash, interner->equality_fn);
    if(entry->value != nullptr) return entry->value;

    void *copy = malloc(interner->value_size);
    memcpy(copy, value, interner->value_size);

    entry->value = copy;
    entry->value_hash = hash;

    interner->count++;
    return entry->value;
}

void interner_free(interner_t *interner, void *value) {
    uint64_t hash = interner->hash_fn(value, interner->value_size);
    entry_t *entry = find_entry(interner->entries, interner->capacity, value, interner->value_size, hash, interner->equality_fn);
    if(entry->value == nullptr) return;

    free(entry->value);
    entry->value = nullptr;
    entry->value_hash = 0;
}

size_t interner_count(interner_t *interner) {
    return interner->count;
}
