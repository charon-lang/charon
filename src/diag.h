#pragma once
#include <stddef.h>
#include <stdarg.h>
#include "source.h"

typedef struct {
    bool present;
    source_t *source;
    size_t offset;
} diag_loc_t;

void diag_error(diag_loc_t diag_loc, char *fmt, ...);
void diag_warn(diag_loc_t diag_loc, char *fmt, ...);