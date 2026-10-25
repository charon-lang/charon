#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t (*hashmap_hash_fn_t)(const void *key);
typedef bool (*hashmap_equal_fn_t)(const void *a, const void *b);
typedef void (*hashmap_visit_fn_t)(const void *key, void *value, void *context);

/// Keys are stored by reference; the owning caller must keep them alive and stable for the map's lifetime. Hashing and equality are supplied per-map.
typedef struct hashmap hashmap_t;

hashmap_t *hashmap_new(hashmap_hash_fn_t hash_fn, hashmap_equal_fn_t equal_fn);
void hashmap_destroy(hashmap_t *map);

/// @brief Look up a key
/// @return the stored value or `nullptr` if absent.
void *hashmap_get(hashmap_t *map, const void *key);

/// @brief Insert or overwrite the value for a key.
void hashmap_set(hashmap_t *map, const void *key, void *value);

/// @brief Remove a key
/// @return the previously stored value or `nullptr` if absent.
void *hashmap_remove(hashmap_t *map, const void *key);

/// @brief Visit every live entry
/// @note The map must not be mutated during iteration.
void hashmap_foreach(hashmap_t *map, hashmap_visit_fn_t visit_fn, void *context);

/// @brief Number of live entries.
size_t hashmap_count(hashmap_t *map);
