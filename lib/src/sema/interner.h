#pragma once

#include "charon/interner.h"

#include <stddef.h>
#include <stdint.h>

#define INTERNER_INIT(LABEL, CLEANUP_FN, CLEANUP_DATA)                                                                                                                              \
    (interner_t) {                                                                                                                                                                  \
        .label = (LABEL), .capacity = 0, .top_used_index = 0, .entries = nullptr, .free_count = 0, .free_entries = nullptr, .cleanup = (CLEANUP_FN), .cleanup_data = (CLEANUP_DATA) \
    }

typedef struct {
    uint32_t revision;
    uint32_t ref_count;
    void *data;
} interner_entry_t;

typedef struct {
    const char *label;

    void *cleanup_data;
    void (*cleanup)(void *cleanup_data, void *data);

    size_t capacity;
    size_t top_used_index;
    interner_entry_t *entries;

    size_t free_count;
    interner_entry_t *free_entries;
} interner_t;

void *interner_lookup(interner_t *interner, charon_interner_key_t key);
charon_interner_key_t interner_insert(interner_t *interner, void *data);

void interner_ref(interner_t *interner, charon_interner_key_t key);
void interner_unref(interner_t *interner, charon_interner_key_t key);

void interner_remove(interner_t *interner, charon_interner_key_t key);
