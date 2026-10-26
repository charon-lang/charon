#pragma once

#include "common/text.h"
#include "core/db.h"
#include "core/store.h"
#include "query/query.h"

#include <stddef.h>

typedef struct {
    const text_t *path;
    store_handle_t handle;
} file_t;

typedef enum {
    QUERY_INPUT_FILE,
} query_input_kind_t;

struct query_input {
    query_input_kind_t kind;
    const void *ptr;
};

typedef struct query_key {
    query_kind_t kind;
    query_input_t input;
} query_key_t;

/// @brief Register an in memory source file. re-registering the same path returns the same file and replaces its source.
const file_t *db_add_file_text(db_t *db, const char *path, const char *src, size_t len);

/// @brief Register a file by reading its contents from disk
const file_t *db_add_file(db_t *db, const char *path);

/// @brief Replace an existing file's source
void db_set_file_source(db_t *db, const file_t *file, const char *src, size_t len);

/// @brief Read a file's current source using the `QUERY_FILE_TEXT` query
const text_t *db_file_source(db_t *db, const file_t *file);

/// @brief resolve a path_t.file handle back to its file_t, or nullptr for the "unknown" handle.
const file_t *db_file_from_handle(db_t *db, store_handle_t handle);

/// @brief create/tear down the file interner
void query_input_init(db_t *db);
void query_input_deinit(db_t *db);
