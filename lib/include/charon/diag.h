#pragma once

#include "charon/path.h"
#include "charon/token.h"

#include <stddef.h>

typedef enum {
#define DIAGNOSTIC(ID, ...) CHARON_DIAG_##ID,
#include "charon/diagnostics.def"
#undef DIAGNOSTIC
} charon_diag_t;

typedef union {
    struct {
        charon_token_kind_t found;
        size_t expected_count;
        charon_token_kind_t expected[];
    } unexpected_token;
} charon_diag_data_t;

typedef struct charon_diag_item {
    charon_diag_t kind;
    charon_diag_data_t *data;

    charon_path_t *path;

    struct charon_diag_item *next;
} charon_diag_item_t;

const char *charon_diag_tostring(charon_diag_t diag);
char *charon_diag_fmt(charon_diag_t diag, charon_diag_data_t *data);
