#include "context.h"

#include "charon/context.h"

#include <stddef.h>

charon_context_t *context_make() {
    charon_context_t *ctx = malloc(sizeof(charon_context_t));
    ctx->file_id_counter = 0;
    ctx->file_entries.element_count = 0;
    ctx->file_entries.elements = nullptr;
    return ctx;
}

void context_destroy(charon_context_t *ctx) {
    free(ctx->file_entries.elements);
    free(ctx);
}

const charon_file_t *context_lookup_file(charon_context_t *ctx, size_t file_id) {
    for(size_t i = 0; i < ctx->file_entries.element_count; i++) {
        if(ctx->file_entries.elements[i].file_id != file_id) continue;
        return ctx->file_entries.elements[i].file;
    }
    return nullptr;
}
