#pragma once

#include "core/store.h"

#include <stddef.h>
#include <stdlib.h>

typedef struct {
    store_handle_t file;
    size_t length;
    size_t indices[];
} path_t;

static inline path_t *path_make(size_t length) {
    path_t *path = malloc(sizeof(path_t) + sizeof(size_t) * length);
    path->length = length;
    return path;
}

static inline void path_destroy(path_t *path) {
    free(path);
}
