#include "query.h"

#include "common/hasher.h"
#include "common/hashmap.h"
#include "common/list.h"
#include "core/db.h"
#include "core/interner.h"
#include "core/store.h"
#include "query/input.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct query_memo {
    query_key_t *key;

    store_handle_t result;
    /// revision at which dependencies were last confirmed clean
    uint64_t verified_at;
    /// revision at which the value last changed
    uint64_t changed_at;

    /// dependency keys read during the last execution
    query_key_t **deps;
    size_t dep_count;

    bool has_run;
    bool in_progress;

    list_node_t stack_node;
} query_memo_t;

void query_drop_none(void *) {}

static uint64_t size_query_key(const void *value) {
    (void) value;
    return sizeof(query_key_t);
}

static uint64_t hash_query_key(const void *value, size_t value_size) {
    (void) value_size;

    const query_key_t *key = value;
    hasher_t hasher = hasher_new();
    hasher_hash(&hasher, key->kind);
    hasher_hash(&hasher, key->input.kind);
    hasher_hash(&hasher, (uint64_t) (uintptr_t) key->input.ptr);
    return hasher_finalize(hasher);
}

static bool equal_query_key(const void *a, const void *b, size_t value_size) {
    (void) value_size;

    const query_key_t *x = a;
    const query_key_t *y = b;
    return x->kind == y->kind && x->input.kind == y->input.kind && x->input.ptr == y->input.ptr;
}

static uint64_t hashmap_key_hash(const void *key) {
    return hash_query_key(key, 0);
}

static bool hashmap_key_equal(const void *a, const void *b) {
    return equal_query_key(a, b, 0);
}

static query_memo_t *memo_fetch(db_t *db, query_kind_t kind, const query_input_t *input, query_key_t **out_key) {
    query_key_t key = { .kind = kind, .input = *input };
    query_key_t *interned = (query_key_t *) interner_intern(db->query_interner, &key);
    *out_key = interned;

    query_memo_t *memo = hashmap_get(db->query.cache, interned);
    if(memo == nullptr) {
        memo = calloc(1, sizeof(query_memo_t));
        memo->key = interned;
        hashmap_set(db->query.cache, interned, memo);
    }
    return memo;
}

static void record_dependency(db_t *db, query_key_t *key) {
    if(db->query.in_progress_memos.count == 0) return;

    query_memo_t *frame = LIST_CONTAINER_OF(db->query.in_progress_memos.tail, query_memo_t, stack_node);
    if(!frame->in_progress) return;

    frame->deps = reallocarray(frame->deps, frame->dep_count + 1, sizeof(query_key_t *));
    frame->deps[frame->dep_count++] = key;
}

const void *query_execute(db_t *db, query_kind_t kind, const query_input_t *input) {
    const query_desc_t *desc = query_descriptor(kind);
    assert(desc != nullptr);

    query_key_t *key;
    query_memo_t *memo = memo_fetch(db, kind, input, &key);

    record_dependency(db, key);

    if(desc->is_input) {
        assert(memo->has_run);
        return store_get(db->store, memo->result);
    }

    if(memo->in_progress) {
        fprintf(stderr, "charon: query cycle detected at '%s'\n", desc->name);
        assert(false);
    }

    if(memo->has_run && memo->verified_at == db->query.revision) {
        db->query.hits++;
        return store_get(db->store, memo->result);
    }

    /// Push the memo so that reads performed while executing target it
    list_push_back(&db->query.in_progress_memos, &memo->stack_node);

    /// Re-verify recorded dependencies.
    bool dirty = !memo->has_run;
    for(size_t i = 0; i < memo->dep_count && !dirty; i++) {
        query_key_t *dep = memo->deps[i];
        query_execute(db, dep->kind, &dep->input);

        query_memo_t *dep_memo = hashmap_get(db->query.cache, dep);
        assert(dep_memo != nullptr);
        if(dep_memo->changed_at > memo->verified_at) dirty = true;
    }

    if(!dirty) {
        memo->verified_at = db->query.revision;
        list_pop_back(&db->query.in_progress_memos);
        db->query.hits++;
        return store_get(db->store, memo->result);
    }

    /// Recompute from scratch, rebuilding the dependency set.
    free(memo->deps);
    memo->deps = nullptr;
    memo->dep_count = 0;

    memo->in_progress = true;
    void *value = desc->exec(db, input);
    memo->in_progress = false;

    list_pop_back(&db->query.in_progress_memos);

    if(memo->has_run) store_unref(db->store, memo->result);

    store_drop_fn_t drop = desc->drop != nullptr ? desc->drop : query_drop_none;
    memo->result = store_insert(db->store, value, drop);
    memo->has_run = true;
    memo->verified_at = db->query.revision;
    memo->changed_at = db->query.revision;
    db->query.misses++;

    return value;
}

void query_set_input(db_t *db, query_kind_t kind, const query_input_t *input, void *value, store_drop_fn_t drop) {
    const query_desc_t *desc = query_descriptor(kind);
    assert(desc != nullptr && desc->is_input);

    query_key_t *key;
    query_memo_t *memo = memo_fetch(db, kind, input, &key);

    if(memo->has_run) store_unref(db->store, memo->result);

    memo->result = store_insert(db->store, value, drop != nullptr ? drop : query_drop_none);
    memo->has_run = true;
    memo->verified_at = ++db->query.revision;
    memo->changed_at = db->query.revision;
}

void query_invalidate(db_t *db, query_kind_t kind, const query_input_t *input) {
    const query_desc_t *desc = query_descriptor(kind);
    assert(desc != nullptr && !desc->is_input);

    query_key_t *key;
    query_memo_t *memo = memo_fetch(db, kind, input, &key);

    uint64_t revision = ++db->query.revision;

    if(memo->has_run) {
        store_unref(db->store, memo->result);
        memo->has_run = false;
    }

    free(memo->deps);
    memo->deps = nullptr;
    memo->dep_count = 0;

    memo->verified_at = 0;
    memo->changed_at = revision;
}

static void memo_free(const void *key, void *value, void *context) {
    (void) key;
    (void) context;

    query_memo_t *memo = value;
    free(memo->deps);
    free(memo);
}

void query_system_init(db_t *db) {
    db->query_interner = interner_new(0, 0, size_query_key, hash_query_key, equal_query_key);
    db->query.cache = hashmap_new(hashmap_key_hash, hashmap_key_equal);
    db->query.revision = 0;
    db->query.in_progress_memos = LIST_INIT;
    db->query.hits = 0;
    db->query.misses = 0;
}

void query_system_deinit(db_t *db) {
    hashmap_foreach(db->query.cache, memo_free, nullptr);
    hashmap_destroy(db->query.cache);
    interner_destroy(db->query_interner);
}
