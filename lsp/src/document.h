#pragma once

#include "linedb.h"

#include <charon/element.h>
#include <charon/memory.h>
#include <stddef.h>

typedef struct {
    char *uri;

    charon_memory_allocator_t *allocator;
    charon_element_cache_t *cache;

    size_t text_length;
    char *text;

    linedb_t linedb;
    const charon_element_inner_t *root_element;
} document_t;

document_t *document_get(const char *uri);
void document_free(document_t *file);
