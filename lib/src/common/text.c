#include "text.h"

#include "common/hasher.h"

#include <stddef.h>
#include <string.h>

size_t text_size(const text_t *text) {
    return sizeof(text_t) + text->size;
}

bool text_equal(const text_t *a, const text_t *b) {
    if(a->size != b->size) return false;
    return memcmp(a->data, b->data, a->size);
}

uint64_t text_hash(const text_t *text) {
    hasher_t hasher = hasher_new();
    for(size_t i = 0; i < text->size; i++) hasher_hash(&hasher, text->data[i]);
    return hasher_finalize(hasher);
}
