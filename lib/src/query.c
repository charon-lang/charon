#include "charon/query.h"

#include "charon/context.h"
#include "common/fatal.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define QUERY_DEFAULT_BUCKET_COUNT 128

typedef struct charon_query_state charon_query_state_t;

typedef struct {
    const charon_query_descriptor_t *descriptor;
    size_t bucket_count;
    charon_query_state_t **buckets;
} charon_query_descriptor_state_t;

struct charon_query_engine {
    charon_memory_allocator_t *allocator;
    charon_context_t *context;

    charon_query_descriptor_state_t *descriptors;
    size_t descriptor_count;
    size_t descriptor_capacity;

    charon_query_state_t **stack;
    size_t stack_size;
    size_t stack_capacity;
};

struct charon_query_state {
    const charon_query_descriptor_t *descriptor;
    uint64_t hash;

    void *key;
    void *value;

    bool has_value;
    bool active;
    bool invalidating;

    charon_query_state_t **dependencies;
    size_t dependency_count;
    size_t dependency_capacity;

    charon_query_state_t **dependents;
    size_t dependent_count;
    size_t dependent_capacity;

    charon_query_state_t *next;
};

static uint64_t default_hash(const void *data, size_t size) {
    const uint8_t *bytes = data;
    const uint64_t p = 0x100000001b3ULL;
    uint64_t h = 0xcbf29ce484222325ULL;
    for(size_t i = 0; i < size; ++i) {
        h ^= bytes[i];
        h *= p;
    }
    return h;
}

static uint64_t hash_key(const charon_query_descriptor_t *descriptor, const void *key) {
    if(descriptor->hash != nullptr) return descriptor->hash(key);
    if(descriptor->key_size == 0 || key == nullptr) return 0;
    return default_hash(key, descriptor->key_size);
}

static bool keys_equal(const charon_query_descriptor_t *descriptor, const void *lhs, const void *rhs) {
    if(descriptor->equals != nullptr) return descriptor->equals(lhs, rhs);
    if(descriptor->key_size == 0) return true;
    return memcmp(lhs, rhs, descriptor->key_size) == 0;
}

static void pointer_array_push(charon_query_engine_t *engine, charon_query_state_t ***array, size_t *count, size_t *capacity, charon_query_state_t *value) {
    if(*count == *capacity) {
        size_t new_capacity = *capacity == 0 ? 4 : (*capacity * 2);
        *array = charon_memory_allocate_array(engine->allocator, *array, new_capacity, sizeof(charon_query_state_t *));
        *capacity = new_capacity;
    }
    (*array)[(*count)++] = value;
}

static void pointer_array_remove(charon_query_state_t ***array, size_t *count, charon_query_state_t *value) {
    if(*count == 0) return;
    for(size_t i = 0; i < *count; ++i) {
        if((*array)[i] != value) continue;
        memmove(&(*array)[i], &(*array)[i + 1], ((*count) - i - 1) * sizeof(charon_query_state_t *));
        --(*count);
        return;
    }
}

static void engine_stack_push(charon_query_engine_t *engine, charon_query_state_t *state) {
    if(engine->stack_size == engine->stack_capacity) {
        size_t new_capacity = engine->stack_capacity == 0 ? 8 : engine->stack_capacity * 2;
        engine->stack = charon_memory_allocate_array(engine->allocator, engine->stack, new_capacity, sizeof(charon_query_state_t *));
        engine->stack_capacity = new_capacity;
    }
    engine->stack[engine->stack_size++] = state;
}

static void engine_stack_pop(charon_query_engine_t *engine) {
    assert(engine->stack_size != 0);
    engine->stack_size--;
}

static charon_query_descriptor_state_t *engine_get_descriptor_state(charon_query_engine_t *engine, const charon_query_descriptor_t *descriptor, bool create) {
    for(size_t i = 0; i < engine->descriptor_count; ++i) {
        if(engine->descriptors[i].descriptor == descriptor) return &engine->descriptors[i];
    }

    if(!create) return nullptr;

    if(engine->descriptor_count == engine->descriptor_capacity) {
        size_t new_capacity = engine->descriptor_capacity == 0 ? 4 : engine->descriptor_capacity * 2;
        engine->descriptors = charon_memory_allocate_array(engine->allocator, engine->descriptors, new_capacity, sizeof(charon_query_descriptor_state_t));
        engine->descriptor_capacity = new_capacity;
    }

    charon_query_descriptor_state_t *state = &engine->descriptors[engine->descriptor_count++];
    state->descriptor = descriptor;
    state->bucket_count = QUERY_DEFAULT_BUCKET_COUNT;
    state->buckets = charon_memory_allocate_array(engine->allocator, nullptr, state->bucket_count, sizeof(charon_query_state_t *));
    for(size_t i = 0; i < state->bucket_count; ++i) state->buckets[i] = nullptr;

    return state;
}

static charon_query_state_t *descriptor_state_find(charon_query_descriptor_state_t *descriptor_state, const void *key, uint64_t hash) {
    size_t bucket = descriptor_state->bucket_count == 0 ? 0 : (hash % descriptor_state->bucket_count);
    charon_query_state_t *state = descriptor_state->buckets[bucket];
    while(state != nullptr) {
        if(state->hash == hash && keys_equal(descriptor_state->descriptor, state->key, key)) return state;
        state = state->next;
    }
    return nullptr;
}

static charon_query_state_t *descriptor_state_insert(charon_query_engine_t *engine, charon_query_descriptor_state_t *descriptor_state, const void *key, uint64_t hash) {
    const charon_query_descriptor_t *descriptor = descriptor_state->descriptor;
    charon_query_state_t *state = charon_memory_allocate(engine->allocator, sizeof(charon_query_state_t));
    state->descriptor = descriptor;
    state->hash = hash;
    state->has_value = false;
    state->active = false;
    state->invalidating = false;
    state->dependencies = nullptr;
    state->dependency_count = 0;
    state->dependency_capacity = 0;
    state->dependents = nullptr;
    state->dependent_count = 0;
    state->dependent_capacity = 0;

    state->key = descriptor->key_size == 0 ? nullptr : charon_memory_allocate(engine->allocator, descriptor->key_size);
    if(descriptor->key_size != 0) memcpy(state->key, key, descriptor->key_size);

    state->value = descriptor->value_size == 0 ? nullptr : charon_memory_allocate(engine->allocator, descriptor->value_size);

    size_t bucket = descriptor_state->bucket_count == 0 ? 0 : (hash % descriptor_state->bucket_count);
    state->next = descriptor_state->buckets[bucket];
    descriptor_state->buckets[bucket] = state;

    return state;
}

static void query_state_remove_from_dependencies(charon_query_state_t *state) {
    for(size_t i = 0; i < state->dependency_count; ++i) {
        charon_query_state_t *dependency = state->dependencies[i];
        pointer_array_remove(&dependency->dependents, &dependency->dependent_count, state);
    }
    state->dependency_count = 0;
}

static void query_state_remove_from_dependents(charon_query_state_t *state) {
    for(size_t i = 0; i < state->dependent_count; ++i) {
        charon_query_state_t *dependent = state->dependents[i];
        pointer_array_remove(&dependent->dependencies, &dependent->dependency_count, state);
    }
    state->dependent_count = 0;
}

static void query_state_add_dependency(charon_query_engine_t *engine, charon_query_state_t *parent, charon_query_state_t *dependency) {
    if(parent == dependency) return;
    for(size_t i = 0; i < parent->dependency_count; ++i) {
        if(parent->dependencies[i] == dependency) return;
    }
    pointer_array_push(engine, &parent->dependencies, &parent->dependency_count, &parent->dependency_capacity, dependency);
    pointer_array_push(engine, &dependency->dependents, &dependency->dependent_count, &dependency->dependent_capacity, parent);
}

static bool query_state_evaluate(charon_query_engine_t *engine, charon_query_state_t *state) {
    if(state->active) {
        fatal("cycle detected while executing query `%s`\n", state->descriptor->name);
    }
    state->active = true;
    engine_stack_push(engine, state);

    query_state_remove_from_dependencies(state);

    bool ok = state->descriptor->compute(engine, engine->context, state->key, state->value);

    if(ok) {
        state->has_value = true;
    } else {
        state->has_value = false;
    }

    engine_stack_pop(engine);
    state->active = false;

    return ok;
}

static bool query_state_ensure(charon_query_engine_t *engine, charon_query_state_t *state) {
    if(state->has_value) return true;
    return query_state_evaluate(engine, state);
}

static void query_state_invalidate_recursive(charon_query_state_t *state) {
    if(state->invalidating) return;
    if(state->active) {
        fatal("cannot invalidate query `%s` while it is executing\n", state->descriptor->name);
    }
    state->invalidating = true;

    if(state->has_value) {
        if(state->descriptor->value_drop != nullptr) state->descriptor->value_drop(state->value);
        state->has_value = false;
    }

    for(size_t i = 0; i < state->dependent_count; ++i) {
        query_state_invalidate_recursive(state->dependents[i]);
    }

    state->invalidating = false;
}

static void query_state_destroy(charon_query_engine_t *engine, charon_query_state_t *state) {
    query_state_remove_from_dependencies(state);
    query_state_remove_from_dependents(state);
    if(state->descriptor->value_drop != nullptr && state->has_value) state->descriptor->value_drop(state->value);
    if(state->descriptor->key_drop != nullptr && state->key != nullptr) state->descriptor->key_drop(state->key);

    if(state->key != nullptr) charon_memory_free(engine->allocator, state->key);
    if(state->value != nullptr) charon_memory_free(engine->allocator, state->value);
    if(state->dependencies != nullptr) charon_memory_free(engine->allocator, state->dependencies);
    if(state->dependents != nullptr) charon_memory_free(engine->allocator, state->dependents);

    charon_memory_free(engine->allocator, state);
}

charon_query_engine_t *charon_query_engine_make(charon_memory_allocator_t *allocator, charon_context_t *context) {
    assert(allocator != nullptr);
    charon_query_engine_t *engine = charon_memory_allocate(allocator, sizeof(charon_query_engine_t));
    engine->allocator = allocator;
    engine->context = context;
    engine->descriptors = nullptr;
    engine->descriptor_count = 0;
    engine->descriptor_capacity = 0;
    engine->stack = nullptr;
    engine->stack_size = 0;
    engine->stack_capacity = 0;
    return engine;
}

void charon_query_engine_destroy(charon_query_engine_t *engine) {
    if(engine == nullptr) return;
    for(size_t i = 0; i < engine->descriptor_count; ++i) {
        charon_query_descriptor_state_t *descriptor_state = &engine->descriptors[i];
        for(size_t bucket = 0; bucket < descriptor_state->bucket_count; ++bucket) {
            charon_query_state_t *state = descriptor_state->buckets[bucket];
            while(state != nullptr) {
                charon_query_state_t *next = state->next;
                query_state_destroy(engine, state);
                state = next;
            }
        }
        charon_memory_free(engine->allocator, descriptor_state->buckets);
    }
    if(engine->descriptors != nullptr) charon_memory_free(engine->allocator, engine->descriptors);
    if(engine->stack != nullptr) charon_memory_free(engine->allocator, engine->stack);
    charon_memory_free(engine->allocator, engine);
}

bool charon_query_engine_execute(charon_query_engine_t *engine, const charon_query_descriptor_t *descriptor, const void *key, const void **value_out) {
    assert(engine != nullptr);
    assert(descriptor != nullptr);
    assert(descriptor->compute != nullptr);
    assert(descriptor->key_size == 0 || key != nullptr);

    charon_query_descriptor_state_t *descriptor_state = engine_get_descriptor_state(engine, descriptor, true);
    uint64_t hash = hash_key(descriptor, key);
    charon_query_state_t *state = descriptor_state_find(descriptor_state, key, hash);
    if(state == nullptr) state = descriptor_state_insert(engine, descriptor_state, key, hash);

    if(engine->stack_size != 0) {
        charon_query_state_t *parent = engine->stack[engine->stack_size - 1];
        query_state_add_dependency(engine, parent, state);
    }

    bool ok = query_state_ensure(engine, state);
    if(!ok) return false;

    if(value_out != nullptr) *value_out = descriptor->value_size == 0 ? nullptr : state->value;
    return true;
}

static void descriptor_state_invalidate_all(charon_query_descriptor_state_t *descriptor_state) {
    for(size_t bucket = 0; bucket < descriptor_state->bucket_count; ++bucket) {
        for(charon_query_state_t *state = descriptor_state->buckets[bucket]; state != nullptr; state = state->next) {
            query_state_invalidate_recursive(state);
        }
    }
}

void charon_query_engine_invalidate(charon_query_engine_t *engine, const charon_query_descriptor_t *descriptor, const void *key) {
    assert(descriptor != nullptr);
    assert(descriptor->key_size == 0 || key != nullptr);
    charon_query_descriptor_state_t *descriptor_state = engine_get_descriptor_state(engine, descriptor, false);
    if(descriptor_state == nullptr) return;
    uint64_t hash = hash_key(descriptor, key);
    charon_query_state_t *state = descriptor_state_find(descriptor_state, key, hash);
    if(state == nullptr) return;
    query_state_invalidate_recursive(state);
}

void charon_query_engine_invalidate_descriptor(charon_query_engine_t *engine, const charon_query_descriptor_t *descriptor) {
    assert(descriptor != nullptr);
    charon_query_descriptor_state_t *descriptor_state = engine_get_descriptor_state(engine, descriptor, false);
    if(descriptor_state == nullptr) return;
    descriptor_state_invalidate_all(descriptor_state);
}

void charon_query_engine_invalidate_all(charon_query_engine_t *engine) {
    for(size_t i = 0; i < engine->descriptor_count; ++i) {
        descriptor_state_invalidate_all(&engine->descriptors[i]);
    }
}
