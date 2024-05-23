#pragma once

#include <stddef.h>

typedef struct {
    const char *name;
    const char *data_buffer;
    size_t data_buffer_size;
} source_t;

typedef struct {
    source_t *source;
    size_t offset;
} source_location_t;

source_t *source_from_path(const char *path);
void source_free(source_t *source);