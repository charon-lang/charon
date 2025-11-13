#pragma once

#include <stddef.h>
#include <stdlib.h>

typedef struct {
    size_t length;
    size_t rel_start, rel_end;
    size_t steps[];
} charon_path_t;

static inline charon_path_t *charon_path_make(size_t step_count) {
    charon_path_t *path = malloc(sizeof(charon_path_t) + sizeof(size_t) * step_count);
    path->length = step_count;
    return path;
}

static inline void charon_path_destroy(charon_path_t *path) {
    free(path);
}
