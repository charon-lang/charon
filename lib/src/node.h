#pragma once

#include "charon/file.h"
#include "charon/path.h"
#include "common//dynarray.h"

#include <stddef.h>

typedef struct {
    const charon_file_t *file;
    charon_path_t *path;
} node_ref_t;

typedef DYNARRAY(node_ref_t *) node_ref_map_t;
