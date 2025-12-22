#pragma once

#include "charon/interner.h"

#include <stddef.h>
#include <stdlib.h>

typedef struct {
    charon_interner_key_t file;
    size_t length;
    size_t indices[];
} charon_path_t;

static inline charon_path_t *charon_path_make(size_t length) {
    charon_path_t *path = malloc(sizeof(charon_path_t) + sizeof(size_t) * length);
    path->length = length;
    return path;
}

static inline void charon_path_destroy(charon_path_t *path) {
    free(path);
}
