#include "charon/element.h"

#include "charon/token.h"
#include "common/memory.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define INTERNED_NODE_BUCKET_COUNT 8096
#define INTERNED_TOKEN_BUCKET_COUNT 8096

typedef struct interned_element {
    struct interned_element *next;
    charon_element_t element;
} interned_element_t;

struct charon_element_cache {
    memory_allocator_t *allocator;
    interned_element_t *token_buckets[INTERNED_TOKEN_BUCKET_COUNT];
    interned_element_t *node_buckets[INTERNED_NODE_BUCKET_COUNT];
};

static uint64_t hash_token(charon_token_kind_t kind, const char *text) {
    const uint64_t p = 0x100000001b3ULL;

    uint64_t h = 0xcbf29ce484222325ULL;
    h ^= (uint64_t) kind;
    h *= p;

    for(size_t i = 0; i < (text == nullptr ? 0 : strlen(text)); ++i) {
        h ^= (uint8_t) text[i];
        h *= p;
    }

    return h;
}

static uint64_t hash_node(charon_node_kind_t kind, charon_element_t *children[], size_t child_count) {
    const uint64_t p = 0x100000001b3ULL;

    uint64_t h = 0xcbf29ce484222325ULL;
    h ^= (uint64_t) kind;
    h *= p;

    h ^= (uint64_t) child_count;
    h *= p;

    for(size_t i = 0; i < child_count; ++i) {
        h ^= children[i]->hash;
        h *= p;
    }

    return h;
}

charon_element_cache_t *charon_element_cache_make() {
    memory_allocator_t *allocator = memory_allocator_make();
    charon_element_cache_t *cache = memory_allocate(allocator, sizeof(charon_element_cache_t));
    cache->allocator = allocator;
    for(size_t i = 0; i < INTERNED_TOKEN_BUCKET_COUNT; i++) cache->token_buckets[i] = nullptr;
    for(size_t i = 0; i < INTERNED_NODE_BUCKET_COUNT; i++) cache->node_buckets[i] = nullptr;
    return cache;
}

void charon_element_cache_destroy(charon_element_cache_t *cache) {
    memory_allocator_free(cache->allocator);
}

charon_element_t *charon_element_make_token(charon_element_cache_t *cache, charon_token_kind_t kind, const char *text) {
    uint64_t hash = hash_token(kind, text);
    size_t index = hash % INTERNED_TOKEN_BUCKET_COUNT;

    for(interned_element_t *interned_element = cache->token_buckets[index]; interned_element != NULL; interned_element = interned_element->next) {
        if(interned_element->element.token.kind != kind) continue;
        if(interned_element->element.token.text == nullptr || text == nullptr) {
            if(interned_element->element.token.text != text) continue;
        } else {
            if(strcmp(interned_element->element.token.text, text) != 0) continue;
        }
        return &interned_element->element;
    }

    interned_element_t *interned_element = memory_allocate(cache->allocator, sizeof(interned_element_t));
    interned_element->element.type = CHARON_ELEMENT_TYPE_TOKEN;
    interned_element->element.hash = hash;
    interned_element->element.length = text == nullptr ? 0 : strlen(text);
    interned_element->element.token.kind = kind;
    interned_element->element.token.text = text == nullptr ? nullptr : memory_strdup(cache->allocator, text);

    interned_element->next = cache->token_buckets[index];
    cache->token_buckets[index] = interned_element;

    return &interned_element->element;
}

charon_element_t *charon_element_make_node(charon_element_cache_t *cache, charon_node_kind_t kind, charon_element_t *children[], size_t child_count) {
    uint64_t hash = hash_node(kind, children, child_count);
    size_t index = hash % INTERNED_NODE_BUCKET_COUNT;

    for(interned_element_t *interned_element = cache->node_buckets[index]; interned_element != NULL; interned_element = interned_element->next) {
        assert(interned_element->element.type == CHARON_ELEMENT_TYPE_NODE);
        if(interned_element->element.node.kind != kind || interned_element->element.node.child_count != child_count) continue;
        for(size_t i = 0; i < interned_element->element.node.child_count; i++) {
            if(interned_element->element.node.children[i] != children[i]) goto skip;
        }
        printf("found interned node\n");
        return &interned_element->element;
    skip:
    }

    interned_element_t *interned_element = memory_allocate(cache->allocator, sizeof(interned_element_t) + child_count * sizeof(charon_element_t *));
    interned_element->element.type = CHARON_ELEMENT_TYPE_NODE;
    interned_element->element.hash = hash;
    interned_element->element.length = 0;
    interned_element->element.node.kind = kind;
    interned_element->element.node.child_count = child_count;
    for(size_t i = 0; i < child_count; i++) {
        interned_element->element.length += children[i]->length;
        interned_element->element.node.children[i] = children[i];
    }

    interned_element->next = cache->node_buckets[index];
    cache->node_buckets[index] = interned_element;

    return &interned_element->element;
}
