#include "token.h"

#include "charon/token.h"
#include "common/memory.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define INTERNED_TOKEN_BUCKET_COUNT 8096

typedef struct interned_token {
    charon_token_t token;
    struct interned_token *next;
} interned_token_t;

struct charon_token_cache {
    memory_allocator_t *allocator;
    interned_token_t *buckets[];
};

static uint64_t hash_token(charon_token_kind_t kind, const char *text) {
    const uint64_t fnv_offset = 0xcbf29ce484222325ULL;
    const uint64_t fnv_prime = 0x100000001b3ULL;

    uint64_t h = fnv_offset;
    h ^= (uint64_t) kind;
    h *= fnv_prime;

    for(size_t i = 0; i < (text == nullptr ? 0 : strlen(text)); ++i) {
        h ^= (uint8_t) text[i];
        h *= fnv_prime;
    }

    return h;
}

charon_token_cache_t *charon_token_cache_make() {
    memory_allocator_t *allocator = memory_allocator_make();
    charon_token_cache_t *cache = memory_allocate(allocator, sizeof(charon_token_cache_t) + INTERNED_TOKEN_BUCKET_COUNT * sizeof(interned_token_t *));
    cache->allocator = allocator;
    for(size_t i = 0; i < INTERNED_TOKEN_BUCKET_COUNT; i++) cache->buckets[i] = nullptr;
    return cache;
}

void charon_token_cache_destroy(charon_token_cache_t *cache) {
    memory_allocator_free(cache->allocator);
}

charon_token_t *token_make(charon_token_cache_t *cache, charon_token_kind_t kind, const char *text) {
    uint64_t hash = hash_token(kind, text);
    size_t index = hash % INTERNED_TOKEN_BUCKET_COUNT;

    for(interned_token_t *itok = cache->buckets[index]; itok != NULL; itok = itok->next) {
        if(itok->token.kind != kind) continue;
        if(itok->token.text == nullptr || text == nullptr) {
            if(itok->token.text != text) continue;
        } else {
            if(strcmp(itok->token.text, text) != 0) continue;
        }
        return &itok->token;
    }

    interned_token_t *itok = memory_allocate(cache->allocator, sizeof(interned_token_t));
    itok->token.kind = kind;
    itok->token.text = text == nullptr ? nullptr : memory_strdup(cache->allocator, text);
    itok->next = cache->buckets[index];
    cache->buckets[index] = itok;
    return &itok->token;
}

const char *charon_token_kind_tostring(charon_token_kind_t kind) {
    static const char *translations[] = {
        [CHARON_TOKEN_KIND_EOF] = "(eof)",
        [CHARON_TOKEN_KIND_UNKNOWN] = "(unknown)",
#define TOKEN(ID, NAME, ...) [CHARON_TOKEN_KIND_##ID] = NAME,
#include "charon/tokens.def"
#undef TOKEN
    };
    return translations[kind];
}

bool charon_token_kind_has_content(charon_token_kind_t kind) {
    static bool has_content[] = {
        [CHARON_TOKEN_KIND_EOF] = false,
        [CHARON_TOKEN_KIND_UNKNOWN] = false,
#define TOKEN(ID, _1, _2, HAS_CONTENT) [CHARON_TOKEN_KIND_##ID] = HAS_CONTENT,
#include "charon/tokens.def"
#undef TOKEN
    };
    return has_content[kind];
}
