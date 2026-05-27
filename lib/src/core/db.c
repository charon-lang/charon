#include "db.h"

#include "common/text.h"
#include "core/store.h"
#include "syntax/element.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static size_t size_text(store_t *, const void *text) {
    return text_size(text);
}

static uint64_t hash_text(store_t *, const void *text, size_t) {
    return text_hash(text);
}

static bool equal_text(store_t *, const void *a, const void *b, size_t) {
    return text_equal(a, b);
}

static void drop_text(void *text) {
    free(text);
}

static size_t size_element(store_t *, const void *element) {
    return element_inner_size(element);
}

static uint64_t hash_element(store_t *store, const void *element, size_t) {
    return element_inner_hash(store, element);
}

static bool equal_element(store_t *, const void *a, const void *b, size_t) {
    return element_inner_equal(a, b);
}

static void drop_element(void *) {
    // TODO:
}

db_t *db_new() {
    db_t *db = malloc(sizeof(db_t));
    db->store = store_new();
    db->element_interner = interner_new(0, 0, size_element, hash_element, equal_element, drop_element);
    db->text_interner = interner_new(0, 0, size_text, hash_text, equal_text, drop_text);
    return db;
}

void db_destroy(db_t *db) {
    store_destroy(db->store);
    interner_destroy(db->text_interner);
    interner_destroy(db->element_interner);
    free(db);
}
