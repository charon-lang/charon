#pragma once

#include <stdint.h>

typedef struct {
    uint64_t hash;
} hasher_t;

static inline hasher_t hasher_new() {
    return (hasher_t) { .hash = 0xcbf29ce484222325ULL };
}

static inline void hasher_hash(hasher_t *hasher, uint64_t value) {
    hasher->hash ^= value;
    hasher->hash *= 0x100000001b3ULL;
}

static inline uint64_t hasher_finalize(hasher_t hasher) {
    return hasher.hash;
}
