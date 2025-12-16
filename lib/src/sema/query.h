#pragma once

#include "charon/context.h"
#include "charon/memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct query_engine query_engine_t;
typedef struct query_descriptor query_descriptor_t;

typedef bool (*query_compute_fn_t)(query_engine_t *engine, context_t *ctx, const void *key, void *value_out);
typedef uint64_t (*query_hash_fn_t)(const void *key);
typedef bool (*query_equal_fn_t)(const void *lhs, const void *rhs);
typedef void (*query_drop_fn_t)(void *ptr);

struct query_descriptor {
    const char *name;
    size_t key_size;
    size_t value_size;
    query_hash_fn_t hash;
    query_equal_fn_t equals;
    query_compute_fn_t compute;
    query_drop_fn_t key_drop;
    query_drop_fn_t value_drop;
};

query_engine_t *query_engine_make(charon_memory_allocator_t *allocator, context_t *context);
void query_engine_destroy(query_engine_t *engine);

bool query_engine_execute(query_engine_t *engine, const query_descriptor_t *descriptor, const void *key, const void **value_out);

void query_engine_invalidate(query_engine_t *engine, const query_descriptor_t *descriptor, const void *key);
void query_engine_invalidate_descriptor(query_engine_t *engine, const query_descriptor_t *descriptor);
void query_engine_invalidate_all(query_engine_t *engine);
