#pragma once

#include "core/db.h"
#include "core/store.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t query_kind_t;
typedef struct query_input query_input_t;

typedef struct {
    const char *name;
    bool is_input;

    /// @brief Compute the query's value from its input
    /// @note Only called for derived queries
    /// @note The query engine will eventually pass the pointer back to `drop`
    void *(*exec)(db_t *db, const query_input_t *input);

    /// @brief Release a value previously returned by `exec`, or `nullptr` for values that are interned/shared and therefore never freed.
    store_drop_fn_t drop;
} query_desc_t;

/// @brief fetch the descriptor for a kind
/// @note aborts on an unknown kind.
const query_desc_t *query_descriptor(query_kind_t kind);

/// @brief Evaluate a derived query
const void *query_execute(db_t *db, query_kind_t kind, const query_input_t *input);

/// @brief Set an input query's value and bump the revision
/// @note Takes ownership of `value`, dropping it with `drop` when required
void query_set_input(db_t *db, query_kind_t kind, const query_input_t *input, void *value, store_drop_fn_t drop);

/// @brief Drop one memo
void query_invalidate(db_t *db, query_kind_t kind, const query_input_t *input);

/// @brief No-op drop for interned/shared values.
void query_drop_none(void *value);

/// Create/tear down the engine's per-database state
void query_system_init(db_t *db);
void query_system_deinit(db_t *db);
