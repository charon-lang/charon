#include "db.h"

#include "core/store.h"
#include "syntax/element.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static uint64_t hash_text(const void *text, size_t) {
    return text_hash(text);
}

static bool equal_text(const void *a, const void *b, size_t) {
    return text_equal(a, b);
}

static uint64_t hash_element(const void *element, size_t) {
    return element_inner_hash(element);
}

static bool equal_element(const void *a, const void *b, size_t) {
    return element_inner_equal(a, b);
}

db_t *db_new() {
    db_t *db = malloc(sizeof(db_t));
    db->store = store_new();
    db->text_interner = interner_new(0, sizeof(element_inner_t), hash_element, equal_element);
    db->element_interner = interner_new(0, sizeof(text_t), hash_text, equal_text);
    return db;
}

void db_destroy(db_t *db) {
    store_destroy(db->store);
    interner_destroy(db->text_interner);
    interner_destroy(db->element_interner);
    free(db);
}
