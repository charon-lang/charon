#include "document.h"

#include "linedb.h"

#include <charon/element.h>
#include <charon/memory.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// TODO: use a hashmap eventually
static document_t **g_source_files = nullptr;
static size_t g_source_file_count;

document_t *document_get(const char *uri) {
    for(size_t i = 0; i < g_source_file_count; i++) {
        document_t *file = g_source_files[i];
        if(strcmp(uri, file->uri) != 0) continue;
        return file;
    }

    document_t *new_file = malloc(sizeof(document_t));
    new_file->uri = strdup(uri);
    new_file->allocator = charon_memory_allocator_make();
    new_file->cache = charon_element_cache_make(new_file->allocator);
    new_file->root_element = nullptr;
    new_file->linedb = (linedb_t) { .line_count = 0, .line_starts = nullptr };

    g_source_files = reallocarray(g_source_files, ++g_source_file_count, sizeof(document_t *));
    g_source_files[g_source_file_count - 1] = new_file;

    return new_file;
}

void document_free(document_t *file) {
    for(size_t i = 0; i < g_source_file_count; i++) {
        if(g_source_files[i] != file) continue;

        if(i + 1 != g_source_file_count) memmove(&g_source_files[i], &g_source_files[i + 1], (g_source_file_count - i) * sizeof(document_t *));
        g_source_files = reallocarray(g_source_files, --g_source_file_count, sizeof(document_t *));
        break;
    }

    linedb_clear(&file->linedb);
    charon_element_cache_destroy(file->cache);
    charon_memory_allocator_free(file->allocator);

    free(file->uri);
    free(file);
}
