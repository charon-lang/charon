#pragma once

#include "charon/symbol.h"
#include "query.h"

#include <stddef.h>

#define QUERIES_DECLARE_QUERY(NAME, DESCRIPTOR, KEY_TYPE, VALUE_TYPE)                                          \
    extern const query_descriptor_t DESCRIPTOR;                                                                \
    static inline bool queries_##NAME(query_engine_t *engine, const KEY_TYPE *key, const VALUE_TYPE **value) { \
        return query_engine_execute(engine, &DESCRIPTOR, (const void *) key, (const void **) value);           \
    }

#define QUERIES_DECLARE_QUERY_VALUEPTR(NAME, DESCRIPTOR, KEY_TYPE, VALUE_TYPE)                                \
    extern const query_descriptor_t DESCRIPTOR;                                                               \
    static inline bool queries_##NAME(query_engine_t *engine, const KEY_TYPE *key, const VALUE_TYPE *value) { \
        VALUE_TYPE *value_ptr;                                                                                \
        bool ok = query_engine_execute(engine, &DESCRIPTOR, (const void *) key, (const void **) &value_ptr);  \
        *value = *value_ptr;                                                                                  \
        return ok;                                                                                            \
    }

QUERIES_DECLARE_QUERY(symtab, g_queries_symtab_descriptor, size_t, charon_symbol_table_t);

#undef QUERIES_DECLARE_QUERY
#undef QUERIES_DECLARE_QUERY_VALUEPTR
