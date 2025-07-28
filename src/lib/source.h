#pragma once

#include <stddef.h>

typedef struct {
    const char *name; /* owned */
    const char *data_buffer; /* owned */
    size_t data_buffer_size;
} source_t;

typedef struct {
    source_t *source;
    size_t offset, length;
} source_location_t;

/** @note source takes ownership over data & name */
source_t *source_make(char *name, char *data, size_t data_length);

void source_free(source_t *source);
