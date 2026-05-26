#pragma once

#include "charon/syntax/token.h"
#include "core/path.h"

#include <stddef.h>

typedef enum {
#define DIAGNOSTIC(ID, ...) CHARON_DIAG_##ID,
#include "diagnostics.def"
#undef DIAGNOSTIC
} diag_t;

typedef union {
    struct {
        charon_token_kind_t found;
        size_t expected_count;
        charon_token_kind_t expected[];
    } unexpected_token;
} diag_data_t;

typedef struct diag_item {
    diag_t kind;
    diag_data_t *data;

    path_t *path;

    struct diag_item *next;
} diag_item_t;

const char *diag_tostring(diag_t diag);
char *diag_fmt(diag_t diag, diag_data_t *data);
