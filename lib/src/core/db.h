#pragma once

#include "core/interner.h"
#include "core/store.h"

typedef struct {
    store_t *store;
    interner_t *text_interner;
    interner_t *element_interner;
} db_t;

db_t *db_new();
void db_destroy(db_t *db);
