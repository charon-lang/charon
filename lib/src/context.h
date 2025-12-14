#pragma once

#include "charon/context.h"
#include "charon/file.h"
#include "common//dynarray.h"

#include <stddef.h>

typedef struct {
    size_t file_id;
    const charon_file_t *file;
} context_file_entry_t;

struct charon_context {
    size_t file_id_counter;
    DYNARRAY(context_file_entry_t) file_entries;
};

charon_context_t *context_make();
void context_destroy(charon_context_t *ctx);

const charon_file_t *context_lookup_file(charon_context_t *ctx, size_t file_id);
