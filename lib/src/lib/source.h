#pragma once

#include "charon/source.h"

#include <stddef.h>

typedef struct charon_source {
    const char *name;
    const char *data_buffer;
    size_t data_buffer_size;
} source_t;

typedef struct {
    source_t *source;
    size_t offset, length;
} source_location_t;

source_t *source_make(const char *name, const char *data, size_t data_length);
void source_free(source_t *source);
