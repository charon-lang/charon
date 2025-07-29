#pragma once

#include <stddef.h>

typedef struct charon_source charon_source_t;

charon_source_t *charon_source_make(const char *name, const char *data, size_t data_length);
void charon_source_free(charon_source_t *source);
