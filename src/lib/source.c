#include "source.h"

#include <stdlib.h>

source_t *source_make(char *name, char *data, size_t data_length) {
    source_t *source = malloc(sizeof(source_t));
    source->name = name;
    source->data_buffer = data;
    source->data_buffer_size = data_length;
    return source;
}

void source_free(source_t *source) {
    free((void *) source->name);
    free((void *) source->data_buffer);
    free(source);
}