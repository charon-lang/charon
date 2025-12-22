#pragma once

#include "charon/interner.h"
#include "query.h"

#include <stddef.h>

#define QUERIES_DECLARE_QUERY(NAME, DESCRIPTOR, KEY_TYPE, VALUE_TYPE)                                                         \
    extern const query_descriptor_t DESCRIPTOR;                                                                               \
    static inline query_compute_status_t queries_##NAME(query_engine_t *engine, const KEY_TYPE *key, VALUE_TYPE *value_out) { \
        return query_engine_execute(engine, &DESCRIPTOR, (const void *) key, value_out);                                      \
    }

QUERIES_DECLARE_QUERY(symtab, g_queries_symtab_descriptor, charon_interner_key_t, charon_interner_key_t);

#undef QUERIES_DECLARE_QUERY
#undef QUERIES_DECLARE_QUERY_VALUEPTR
