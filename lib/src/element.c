#include "element.h"

#include "charon/element.h"
#include "charon/memory.h"
#include "charon/token.h"
#include "charon/trivia.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define INTERNED_TRIVIA_BUCKET_COUNT 1024
#define INTERNED_TOKEN_BUCKET_COUNT 16192
#define INTERNED_NODE_BUCKET_COUNT 8096

typedef struct interned_element {
    struct interned_element *next;
    charon_element_inner_t element;
} interned_element_t;

struct charon_element_cache {
    charon_memory_allocator_t *allocator;
    interned_element_t *trivia_buckets[INTERNED_TRIVIA_BUCKET_COUNT];
    interned_element_t *token_buckets[INTERNED_TOKEN_BUCKET_COUNT];
    interned_element_t *node_buckets[INTERNED_NODE_BUCKET_COUNT];
};

static void free_buckets(charon_memory_allocator_t *allocator, interned_element_t *buckets[], size_t bucket_count) {
    for(size_t i = 0; i < bucket_count; i++) {
        interned_element_t *element = buckets[i];
        while(element != nullptr) {
            interned_element_t *tmp = element;
            element = tmp->next;
            charon_memory_free(allocator, tmp);
        }
    }
}

static uint64_t hash_trivia(charon_trivia_kind_t kind, const char *text) {
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

static uint64_t hash_token(charon_token_kind_t kind, const char *text, const charon_element_inner_t *trivia[], size_t trivia_count) {
    const uint64_t p = 0x100000001b3ULL;

    uint64_t h = 0xcbf29ce484222325ULL;
    h ^= (uint64_t) kind;
    h *= p;

    for(size_t i = 0; i < (text == nullptr ? 0 : strlen(text)); ++i) {
        h ^= (uint8_t) text[i];
        h *= p;
    }

    for(size_t i = 0; i < trivia_count; ++i) {
        h ^= trivia[i]->hash;
        h *= p;
    }

    return h;
}

static uint64_t hash_node(charon_node_kind_t kind, const charon_element_inner_t *children[], size_t child_count) {
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

static const charon_element_inner_t *token_trivia(const charon_element_inner_t *inner_element, size_t index) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_TOKEN);
    assert(index < inner_element->token.leading_trivia_count + inner_element->token.trailing_trivia_count);
    return inner_element->token.trivia[index];
}

static charon_element_t *token_trivia_wrap(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index) {
    charon_element_t *trivia = charon_memory_allocate(allocator, sizeof(charon_element_t));
    trivia->parent = element;
    trivia->inner = token_trivia(element->inner, index);
    trivia->offset = element->offset;
    if(index >= element->inner->token.leading_trivia_count) trivia->offset += element->inner->length - (element->inner->token.leading_trivia_length + element->inner->token.trailing_trivia_length);
    for(size_t i = 0; i < index; i++) trivia->offset += element->inner->token.trivia[i]->length;

    return trivia;
}

charon_element_cache_t *charon_element_cache_make(charon_memory_allocator_t *allocator) {
    charon_element_cache_t *cache = charon_memory_allocate(allocator, sizeof(charon_element_cache_t));
    cache->allocator = allocator;
    for(size_t i = 0; i < INTERNED_TRIVIA_BUCKET_COUNT; i++) cache->trivia_buckets[i] = nullptr;
    for(size_t i = 0; i < INTERNED_TOKEN_BUCKET_COUNT; i++) cache->token_buckets[i] = nullptr;
    for(size_t i = 0; i < INTERNED_NODE_BUCKET_COUNT; i++) cache->node_buckets[i] = nullptr;
    return cache;
}

void charon_element_cache_destroy(charon_element_cache_t *cache) {
    free_buckets(cache->allocator, cache->trivia_buckets, INTERNED_TRIVIA_BUCKET_COUNT);
    free_buckets(cache->allocator, cache->token_buckets, INTERNED_TOKEN_BUCKET_COUNT);
    free_buckets(cache->allocator, cache->node_buckets, INTERNED_NODE_BUCKET_COUNT);
    charon_memory_free(cache->allocator, cache);
}

const charon_element_inner_t *charon_element_inner_make_trivia(charon_element_cache_t *cache, charon_trivia_kind_t kind, const char *text) {
    uint64_t hash = hash_trivia(kind, text);
    size_t index = hash % INTERNED_TRIVIA_BUCKET_COUNT;

    for(interned_element_t *interned_element = cache->trivia_buckets[index]; interned_element != NULL; interned_element = interned_element->next) {
        if(interned_element->element.trivia.kind != kind) continue;
        if(interned_element->element.trivia.text == nullptr || text == nullptr) {
            if(interned_element->element.trivia.text != text) continue;
        } else {
            if(strcmp(interned_element->element.trivia.text, text) != 0) continue;
        }
        return &interned_element->element;
    }

    interned_element_t *interned_element = charon_memory_allocate(cache->allocator, sizeof(interned_element_t));
    interned_element->element.type = CHARON_ELEMENT_TYPE_TRIVIA;
    interned_element->element.hash = hash;
    interned_element->element.length = text == nullptr ? 0 : strlen(text);
    interned_element->element.trivia.kind = kind;
    interned_element->element.trivia.text = text == nullptr ? nullptr : charon_memory_strdup(cache->allocator, text);

    interned_element->next = cache->trivia_buckets[index];
    cache->trivia_buckets[index] = interned_element;

    return &interned_element->element;
}

const charon_element_inner_t *charon_element_inner_make_token(charon_element_cache_t *cache, charon_token_kind_t kind, const char *text, size_t leading_trivia_count, size_t trailing_trivia_count, const charon_element_inner_t *trivia[]) {
    uint64_t hash = hash_token(kind, text, trivia, leading_trivia_count + trailing_trivia_count);
    size_t index = hash % INTERNED_TOKEN_BUCKET_COUNT;

    for(interned_element_t *interned_element = cache->token_buckets[index]; interned_element != NULL; interned_element = interned_element->next) {
        if(interned_element->element.token.kind != kind) continue;
        if(interned_element->element.token.text == nullptr || text == nullptr) {
            if(interned_element->element.token.text != text) continue;
        } else {
            if(strcmp(interned_element->element.token.text, text) != 0) continue;
        }

        if(interned_element->element.token.leading_trivia_count != leading_trivia_count) continue;
        if(interned_element->element.token.trailing_trivia_count != trailing_trivia_count) continue;
        for(size_t i = 0; i < leading_trivia_count + trailing_trivia_count; i++) {
            if(interned_element->element.token.trivia[i] != trivia[i]) goto skip;
        }

        return &interned_element->element;
    skip:
    }

    interned_element_t *interned_element = charon_memory_allocate(cache->allocator, sizeof(interned_element_t) + (leading_trivia_count + trailing_trivia_count) * sizeof(charon_element_inner_t *));
    interned_element->element.type = CHARON_ELEMENT_TYPE_TOKEN;
    interned_element->element.hash = hash;
    interned_element->element.length = text == nullptr ? 0 : strlen(text);
    interned_element->element.token.kind = kind;
    interned_element->element.token.text = text == nullptr ? nullptr : charon_memory_strdup(cache->allocator, text);
    interned_element->element.token.leading_trivia_count = leading_trivia_count;
    interned_element->element.token.trailing_trivia_count = trailing_trivia_count;
    interned_element->element.token.leading_trivia_length = 0;
    interned_element->element.token.trailing_trivia_length = 0;
    for(size_t i = 0; i < leading_trivia_count + trailing_trivia_count; i++) {
        interned_element->element.token.trivia[i] = trivia[i];
        interned_element->element.length += trivia[i]->length;
        if(i < leading_trivia_count) {
            interned_element->element.token.leading_trivia_length += trivia[i]->length;
        } else {
            interned_element->element.token.trailing_trivia_length += trivia[i]->length;
        }
    }

    interned_element->next = cache->token_buckets[index];
    cache->token_buckets[index] = interned_element;

    return &interned_element->element;
}

const charon_element_inner_t *charon_element_inner_make_node(charon_element_cache_t *cache, charon_node_kind_t kind, const charon_element_inner_t *children[], size_t child_count) {
    uint64_t hash = hash_node(kind, children, child_count);
    size_t index = hash % INTERNED_NODE_BUCKET_COUNT;

    for(interned_element_t *interned_element = cache->node_buckets[index]; interned_element != NULL; interned_element = interned_element->next) {
        assert(interned_element->element.type == CHARON_ELEMENT_TYPE_NODE);
        if(interned_element->element.node.kind != kind || interned_element->element.node.child_count != child_count) continue;
        for(size_t i = 0; i < interned_element->element.node.child_count; i++) {
            if(interned_element->element.node.children[i] != children[i]) goto skip;
        }
        return &interned_element->element;
    skip:
    }

    interned_element_t *interned_element = charon_memory_allocate(cache->allocator, sizeof(interned_element_t) + child_count * sizeof(charon_element_inner_t *));
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

charon_element_t *charon_element_wrap_root(charon_memory_allocator_t *allocator, const charon_element_inner_t *inner_root) {
    charon_element_t *element = charon_memory_allocate(allocator, sizeof(charon_element_t));
    element->parent = NULL;
    element->offset = 0;
    element->inner = inner_root;
    return element;
}

charon_element_t *charon_element_wrap_node_child(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index) {
    charon_element_t *token = charon_memory_allocate(allocator, sizeof(charon_element_t));
    token->parent = element;
    token->inner = charon_element_node_child(element->inner, index);
    token->offset = element->offset;
    for(size_t i = 0; i < index; i++) token->offset += element->inner->node.children[i]->length;

    return token;
}

charon_element_t *charon_element_wrap_token_leading_trivia(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index) {
    assert(index < element->inner->token.leading_trivia_count);
    return token_trivia_wrap(allocator, element, index);
}

charon_element_t *charon_element_wrap_token_trailing_trivia(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index) {
    assert(index < element->inner->token.trailing_trivia_count);
    return token_trivia_wrap(allocator, element, element->inner->token.leading_trivia_count + index);
}

charon_element_type_t charon_element_type(const charon_element_inner_t *inner_element) {
    return inner_element->type;
}

size_t charon_element_length(const charon_element_inner_t *inner_element) {
    return inner_element->length;
}

const char *charon_element_trivia_text(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_TRIVIA);
    return inner_element->trivia.text;
}

charon_trivia_kind_t charon_element_trivia_kind(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_TRIVIA);
    return inner_element->trivia.kind;
}

const char *charon_element_token_text(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_TOKEN);
    return inner_element->token.text;
}

charon_token_kind_t charon_element_token_kind(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_TOKEN);
    return inner_element->token.kind;
}

size_t charon_element_token_leading_trivia_count(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_TOKEN);
    return inner_element->token.leading_trivia_count;
}

size_t charon_element_token_trailing_trivia_count(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_TOKEN);
    return inner_element->token.trailing_trivia_count;
}

size_t charon_element_token_leading_trivia_length(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_TOKEN);
    return inner_element->token.leading_trivia_length;
}

size_t charon_element_token_trailing_trivia_length(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_TOKEN);
    return inner_element->token.trailing_trivia_length;
}

const charon_element_inner_t *charon_element_token_leading_trivia(const charon_element_inner_t *inner_element, size_t index) {
    assert(index < inner_element->token.leading_trivia_count);
    return token_trivia(inner_element, index);
}

const charon_element_inner_t *charon_element_token_trailing_trivia(const charon_element_inner_t *inner_element, size_t index) {
    assert(index < inner_element->token.trailing_trivia_count);
    return token_trivia(inner_element, inner_element->token.leading_trivia_count + index);
}

charon_node_kind_t charon_element_node_kind(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_NODE);
    return inner_element->node.kind;
}

size_t charon_element_node_child_count(const charon_element_inner_t *inner_element) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_NODE);
    return inner_element->node.child_count;
}

const charon_element_inner_t *charon_element_node_child(const charon_element_inner_t *inner_element, size_t index) {
    assert(inner_element->type == CHARON_ELEMENT_TYPE_NODE);
    assert(index < inner_element->node.child_count);
    return inner_element->node.children[index];
}
