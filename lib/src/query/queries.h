#pragma once

#include "core/diag.h"
#include "syntax/element.h"

#include <stddef.h>

typedef enum {
    /// The file's current source text.
    QUERY_FILE_TEXT,

    /// The parsed root element plus its diagnostics
    QUERY_FILE_PARSE,

    /// The parse's diagnostics as a flat array
    QUERY_FILE_DIAGNOSTICS,

    /// The top-level declarations of the parse
    QUERY_FILE_ITEMS,

    /// Every token in the parse
    QUERY_FILE_TOKENS,

    QUERY_KIND_COUNT,
} query_file_kind_t;

typedef struct {
    const element_inner_t *root;
    diag_item_t *diagnostics;
} query_parse_result_t;

typedef struct {
    size_t count;
    diag_item_t **items;
} diag_array_t;

typedef struct {
    size_t count;
    const element_inner_t **items;
} decl_array_t;

typedef struct {
    size_t count;
    const element_inner_t **tokens;
} element_array_t;
