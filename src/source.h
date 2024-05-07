#pragma once
#include <stddef.h>

typedef struct {
    const char *name;
    size_t data_length;
    const char *data;
} source_t;