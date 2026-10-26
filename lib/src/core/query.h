#pragma once

#include "core/db.h"

#include <stdint.h>

typedef enum {
    QUERY_COMPUTE_STATUS_FAIL,
    QUERY_COMPUTE_STATUS_OK,
} query_compute_status_t;

typedef uint64_t query_revision_t;

typedef struct query_engine query_engine_t;

typedef query_compute_status_t (*query_compute_fn_t)(db_t *db, const void *key, void *value_out);
typedef uint64_t (*query_hash_fn_t)(const void *key);
typedef bool (*query_equal_fn_t)(const void *lhs, const void *rhs);
typedef void (*query_drop_fn_t)(db_t *db, void *ptr);
typedef bool (*query_copy_fn_t)(db_t *db, const void *in, void *out);

typedef struct {
    uint32_t query_id;
    uint64_t input_value;
} query_key_t;

struct query_descriptor {
    const char *name;
    size_t key_size;
    size_t value_size;
    query_hash_fn_t hash;
    query_equal_fn_t equals;
    query_compute_fn_t compute;
    query_drop_fn_t key_drop;
    query_drop_fn_t value_drop;
    query_copy_fn_t value_copy;
};

query_engine_t *query_db_new();
void query_db_destroy(query_engine_t *db);
