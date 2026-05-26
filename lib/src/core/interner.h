#pragma once

#include <stddef.h>
#include <stdint.h>

typedef uint64_t (*interner_hash_fn_t)(const void *value, size_t value_size);
typedef bool (*interner_equality_fn_t)(const void *a, const void *b, size_t value_size);

typedef struct interner interner_t;

interner_t *interner_new(size_t initial_capacity, size_t value_size, interner_hash_fn_t hash_fn, interner_equality_fn_t equality_fn);
void interner_destroy(interner_t *interner);

const void *interner_intern(interner_t *interner, const void *value);
void interner_free(interner_t *interner, void *value);

size_t interner_count(interner_t *interner);
