#include "source.h"

#include <stdlib.h>
#include <string.h>

[[gnu::alias("source_make")]] charon_source_t *charon_source_make(const char *name, const char *data, size_t data_length);
[[gnu::alias("source_free")]] void charon_source_free(charon_source_t *source);

source_t *source_make(const char *name, const char *data, size_t data_length) {
    source_t *source = malloc(sizeof(source_t));
    source->name = strdup(name);
    source->data_buffer = strdup(data);
    source->data_buffer_size = data_length;
    return source;
}

void source_free(source_t *source) {
    free((void *) source->name);
    free((void *) source->data_buffer);
    free(source);
}
