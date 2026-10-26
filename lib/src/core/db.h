#pragma once

#include "common/hashmap.h"
#include "common/list.h"
#include "core/interner.h"
#include "core/store.h"

#include <stdint.h>

typedef struct {
    store_t *store;
    interner_t *text_interner;
    interner_t *element_interner;
    interner_t *file_interner;
    interner_t *query_interner;

    struct {
        /// interned query_key_t* -> query_memo_t*
        hashmap_t *cache;

        uint64_t revision;
        list_t in_progress_memos;
        size_t hits, misses;
    } query;
} db_t;

db_t *db_new();
void db_destroy(db_t *db);
