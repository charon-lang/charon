#pragma once
#include <stddef.h>

typedef struct {
    char *name;
    size_t data_length;
    char *data;
} source_t;