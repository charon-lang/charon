#pragma once

#include <stddef.h>

typedef struct charon_source charon_source_t;

charon_source_t *charon_source_make(const char *name, const char *text, size_t text_length);
void charon_source_destroy(charon_source_t *source);
