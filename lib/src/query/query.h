#pragma once

#include "core/db.h"
#include "core/store.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t query_kind_t;
typedef struct query_input query_input_t;

/// @brief Compare a derived query's previous value against its recomputed value
/// @note Returning true lets the engine back-date changed_at, so dependents are not re-ran
typedef bool (*query_value_equal_fn_t)(const void *old_value, const void *new_value);

typedef struct {
    const char *name;
    bool is_input;

    /// @brief Compute the query's value from its input
    /// @note Only called for derived queries
    /// @note The query engine will eventually pass the pointer back to `drop`
    void *(*exec)(db_t *db, const query_input_t *input);

    /// @brief Release a value previously returned by `exec`, or `nullptr` for values that are interned/shared and therefore never freed.
    store_drop_fn_t drop;

    /// @brief Optional early cutoff comparison, see query_value_equal_fn_t. nullptr will force the engine to always re-run dependents.
    query_value_equal_fn_t value_equal;
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
