#pragma once

#include <stdint.h>

typedef void (*store_drop_fn_t)(void *value);

typedef struct {
    uint32_t index;
    uint32_t revision;
} store_handle_t;

typedef struct store store_t;

store_t *store_new();
void store_destroy(store_t *store);

store_handle_t store_insert(store_t *store, void *data, store_drop_fn_t drop_fn);
void *store_get(store_t *store, store_handle_t handle);

void store_ref(store_t *store, store_handle_t handle);
void store_unref(store_t *store, store_handle_t handle);
