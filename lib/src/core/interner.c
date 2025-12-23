#include "interner.h"

#include "charon/interner.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define EXPAND_COUNT(CURRENT_CAPACITY) ((CURRENT_CAPACITY) == 0 ? 4 : (CURRENT_CAPACITY))
#define INDEX_OF(INTERNER, ENTRY) (((uintptr_t) (ENTRY) - (uintptr_t) (INTERNER)->entries) / sizeof(interner_entry_t))

static void add_free_entry(interner_t *interner, interner_entry_t *entry) {
    entry->ref_count = 0;
    entry->data = interner->free_entries;
    interner->free_entries = entry;
    interner->free_count++;
}

static void expand(interner_t *interner, size_t count) {
    interner->entries = reallocarray(interner->entries, interner->capacity + count, sizeof(interner_entry_t));
    for(size_t i = 0; i < count; i++) {
        interner_entry_t *entry = &interner->entries[interner->capacity + i];
        entry->revision = 0;
        add_free_entry(interner, entry);
    }
    interner->capacity += count;
}

static interner_entry_t *get_free_entry(interner_t *interner) {
    if(interner->free_count == 0) expand(interner, EXPAND_COUNT(interner->capacity));

    assert(interner->free_entries != nullptr);

    interner_entry_t *entry = interner->free_entries;
    interner->free_entries = entry->data;
    entry->data = nullptr;
    interner->free_count--;

    return entry;
}

static interner_entry_t *lookup(interner_t *interner, charon_interner_key_t key) {
    assert(key.index <= interner->top_used_index);

    interner_entry_t *entry = &interner->entries[key.index];
    if(entry->revision > key.revision) return nullptr;

    return entry;
}

void *interner_lookup(interner_t *interner, charon_interner_key_t key) {
    interner_entry_t *entry = lookup(interner, key);
    if(entry == nullptr) return nullptr;
    return entry->data;
}

charon_interner_key_t interner_insert(interner_t *interner, void *data) {
    interner_entry_t *entry = get_free_entry(interner);
    entry->revision++;
    entry->data = data;
    entry->ref_count = 1;

    size_t index = INDEX_OF(interner, entry);
    if(interner->top_used_index < index) {
        interner->top_used_index = index;
    }

    assert(index <= UINT32_MAX);

#ifdef TRACE_INTERNER
    printf("INTERNER `%s` NEW %4lu:%u\n", interner->label, index, entry->revision);
#endif

    return (charon_interner_key_t) {
        .index = index,
        .revision = entry->revision,
    };
}

void interner_ref(interner_t *interner, charon_interner_key_t key) {
    interner_entry_t *entry = lookup(interner, key);

#ifdef TRACE_INTERNER
    printf("INTERNER `%s` REF %4lu:%u\n", interner->label, INDEX_OF(interner, entry), entry->revision);
#endif

    assert(entry != nullptr);
    assert(entry->ref_count > 0);

    entry->ref_count++;
}

void interner_unref(interner_t *interner, charon_interner_key_t key) {
    interner_entry_t *entry = lookup(interner, key);

#ifdef TRACE_INTERNER
    printf("INTERNER `%s` UNREF %4lu:%u\n", interner->label, INDEX_OF(interner, entry), entry->revision);
#endif

    assert(entry != nullptr);
    assert(entry->ref_count > 0);

    if(--entry->ref_count == 0) interner_remove(interner, key);
}

void interner_remove(interner_t *interner, charon_interner_key_t key) {
    interner_entry_t *entry = lookup(interner, key);

    assert(entry != nullptr);
    assert(entry->ref_count == 0);

#ifdef TRACE_INTERNER
    printf("INTERNER `%s` DEL %4u:%u\n", interner->label, key.index, key.revision);
#endif

    if(interner->top_used_index == INDEX_OF(interner, entry)) {
        interner->top_used_index--;
    }

    if(interner->cleanup != nullptr) interner->cleanup(interner->cleanup_data, entry->data);

    entry->revision++;
    add_free_entry(interner, entry);
}
