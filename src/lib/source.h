#pragma once

#include <stddef.h>

typedef struct {
    const char *name;
    const char *data_buffer;
    size_t buffer_size;
} source_t;

source_t *source_from_path(const char *path);
void source_free(source_t *source);