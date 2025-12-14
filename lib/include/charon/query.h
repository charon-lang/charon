#pragma once

#include "charon/context.h"
#include "charon/memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct charon_query_engine charon_query_engine_t;
typedef struct charon_query_descriptor charon_query_descriptor_t;

typedef bool (*charon_query_compute_fn_t)(charon_query_engine_t *engine, charon_context_t *ctx, const void *key, void *value_out);
typedef uint64_t (*charon_query_hash_fn_t)(const void *key);
typedef bool (*charon_query_equal_fn_t)(const void *lhs, const void *rhs);
typedef void (*charon_query_drop_fn_t)(void *ptr);

struct charon_query_descriptor {
    const char *name;
    size_t key_size;
    size_t value_size;
    charon_query_hash_fn_t hash;
    charon_query_equal_fn_t equals;
    charon_query_compute_fn_t compute;
    charon_query_drop_fn_t key_drop;
    charon_query_drop_fn_t value_drop;
};

charon_query_engine_t *charon_query_engine_make(charon_memory_allocator_t *allocator, charon_context_t *context);
void charon_query_engine_destroy(charon_query_engine_t *engine);

bool charon_query_engine_execute(charon_query_engine_t *engine, const charon_query_descriptor_t *descriptor, const void *key, const void **value_out);

void charon_query_engine_invalidate(charon_query_engine_t *engine, const charon_query_descriptor_t *descriptor, const void *key);
void charon_query_engine_invalidate_descriptor(charon_query_engine_t *engine, const charon_query_descriptor_t *descriptor);
void charon_query_engine_invalidate_all(charon_query_engine_t *engine);
