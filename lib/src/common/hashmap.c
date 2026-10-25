#include "hashmap.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define HASHMAP_MINIMUM_CAPACITY 16
#define HASHMAP_MAXIMUM_USAGE 0.75

static const void *const HASHMAP_TOMBSTONE = (const void *) 1;

typedef struct {
    const void *key;
    void *value;
    uint64_t hash;
} hashmap_entry_t;

struct hashmap {
    hashmap_entry_t *entries;
    size_t count;
    size_t tombstones;
    size_t capacity;
    hashmap_hash_fn_t hash_fn;
    hashmap_equal_fn_t equal_fn;
};

static size_t hashmap_slot(hashmap_t *map, uint64_t hash) {
    return (size_t) (hash & (map->capacity - 1));
}

/// @brief: Find the live entry matching "key", or "nullptr" if it is absent.
static hashmap_entry_t *hashmap_lookup(hashmap_t *map, const void *key, uint64_t hash) {
    size_t index = hashmap_slot(map, hash);
    while(true) {
        hashmap_entry_t *entry = &map->entries[index];
        if(entry->key == nullptr) return nullptr;
        if(entry->key != HASHMAP_TOMBSTONE && entry->hash == hash && map->equal_fn(entry->key, key)) return entry;
        index = (index + 1) & (map->capacity - 1);
    }
}

/// @brief: Find the slot where "key" should be inserted, reusing the first tombstone seen.
static hashmap_entry_t *hashmap_find_slot(hashmap_t *map, const void *key, uint64_t hash) {
    size_t index = hashmap_slot(map, hash);
    size_t tombstone = SIZE_MAX;
    while(true) {
        hashmap_entry_t *entry = &map->entries[index];
        if(entry->key == nullptr) return tombstone == SIZE_MAX ? entry : &map->entries[tombstone];
        if(entry->key == HASHMAP_TOMBSTONE) {
            if(tombstone == SIZE_MAX) tombstone = index;
        } else if(entry->hash == hash && map->equal_fn(entry->key, key)) {
            return entry;
        }
        index = (index + 1) & (map->capacity - 1);
    }
}

static void hashmap_resize(hashmap_t *map, size_t new_capacity) {
    hashmap_entry_t *old_entries = map->entries;
    size_t old_capacity = map->capacity;

    map->entries = calloc(new_capacity, sizeof(hashmap_entry_t));
    map->capacity = new_capacity;
    map->count = 0;
    map->tombstones = 0;

    for(size_t i = 0; i < old_capacity; i++) {
        hashmap_entry_t *entry = &old_entries[i];
        if(entry->key == nullptr || entry->key == HASHMAP_TOMBSTONE) continue;

        hashmap_entry_t *slot = hashmap_find_slot(map, entry->key, entry->hash);
        slot->key = entry->key;
        slot->value = entry->value;
        slot->hash = entry->hash;
        map->count++;
    }

    free(old_entries);
}

hashmap_t *hashmap_new(hashmap_hash_fn_t hash_fn, hashmap_equal_fn_t equal_fn) {
    assert(hash_fn != nullptr && equal_fn != nullptr);

    hashmap_t *map = malloc(sizeof(hashmap_t));
    map->entries = calloc(HASHMAP_MINIMUM_CAPACITY, sizeof(hashmap_entry_t));
    map->count = 0;
    map->tombstones = 0;
    map->capacity = HASHMAP_MINIMUM_CAPACITY;
    map->hash_fn = hash_fn;
    map->equal_fn = equal_fn;
    return map;
}

void hashmap_destroy(hashmap_t *map) {
    free(map->entries);
    free(map);
}

void *hashmap_get(hashmap_t *map, const void *key) {
    hashmap_entry_t *entry = hashmap_lookup(map, key, map->hash_fn(key));
    return entry == nullptr ? nullptr : entry->value;
}

void hashmap_set(hashmap_t *map, const void *key, void *value) {
    assert(key != nullptr && key != HASHMAP_TOMBSTONE);

    if((double) (map->count + map->tombstones) / (double) map->capacity > HASHMAP_MAXIMUM_USAGE) hashmap_resize(map, map->capacity * 2);

    uint64_t hash = map->hash_fn(key);

    hashmap_entry_t *entry = hashmap_lookup(map, key, hash);
    if(entry != nullptr) {
        entry->value = value;
        return;
    }

    entry = hashmap_find_slot(map, key, hash);
    if(entry->key == HASHMAP_TOMBSTONE) map->tombstones--;

    entry->key = key;
    entry->value = value;
    entry->hash = hash;
    map->count++;
}

void *hashmap_remove(hashmap_t *map, const void *key) {
    uint64_t hash = map->hash_fn(key);

    hashmap_entry_t *entry = hashmap_lookup(map, key, hash);
    if(entry == nullptr) return nullptr;

    void *value = entry->value;
    entry->key = HASHMAP_TOMBSTONE;
    entry->value = nullptr;
    entry->hash = 0;

    map->count--;
    map->tombstones++;
    return value;
}

void hashmap_foreach(hashmap_t *map, hashmap_visit_fn_t visit_fn, void *context) {
    for(size_t i = 0; i < map->capacity; i++) {
        hashmap_entry_t *entry = &map->entries[i];
        if(entry->key == nullptr || entry->key == HASHMAP_TOMBSTONE) continue;
        visit_fn(entry->key, entry->value, context);
    }
}

size_t hashmap_count(hashmap_t *map) {
    return map->count;
}
