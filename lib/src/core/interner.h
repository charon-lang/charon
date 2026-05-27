#pragma once

#include "core/store.h"

#include <stddef.h>
#include <stdint.h>

typedef uint64_t (*interner_size_fn_t)(store_t *store, const void *value);
typedef uint64_t (*interner_hash_fn_t)(store_t *store, const void *value, size_t value_size);
typedef bool (*interner_equality_fn_t)(store_t *store, const void *a, const void *b, size_t value_size);

typedef struct interner interner_t;

interner_t *interner_new(size_t initial_capacity, size_t value_size, interner_size_fn_t size_fn, interner_hash_fn_t hash_fn, interner_equality_fn_t equality_fn, store_drop_fn_t drop_fn);
void interner_destroy(interner_t *interner);

store_handle_t interner_intern(interner_t *interner, const void *value);
// void interner_free(interner_t *interner, void *value);

size_t interner_count(interner_t *interner);
