#include "charon/builder.h"

#include "charon/element.h"
#include "charon/memory.h"
#include "charon/node.h"
#include "charon/token.h"

#include <assert.h>
#include <stdlib.h>

typedef struct open_node {
    struct open_node *parent;
    charon_node_kind_t kind;
    size_t child_count;
    charon_element_inner_t **children;
} open_node_t;

struct charon_builder {
    charon_element_cache_t *cache;
    open_node_t *current;
};

static open_node_t *open_node_make(open_node_t *parent, charon_node_kind_t kind) {
    open_node_t *node = malloc(sizeof(open_node_t));
    node->kind = kind;
    node->parent = parent;
    node->child_count = 0;
    node->children = NULL;
    return node;
}

static charon_element_inner_t *open_node_build(charon_element_cache_t *cache, open_node_t *node) {
    charon_element_inner_t *inner_element = charon_element_inner_make_node(cache, node->kind, node->children, node->child_count);
    if(node->children != NULL) free(node->children);
    free(node);
    return inner_element;
}

static void open_node_add_child(open_node_t *node, charon_element_inner_t *child) {
    node->children = reallocarray(node->children, ++node->child_count, sizeof(charon_element_inner_t *));
    node->children[node->child_count - 1] = child;
}

charon_builder_t *charon_builder_init(charon_element_cache_t *cache, charon_node_kind_t root_kind) {
    charon_builder_t *builder = malloc(sizeof(charon_builder_t));
    builder->cache = cache;
    builder->current = open_node_make(NULL, root_kind);
    return builder;
}

void charon_builder_node_start(charon_builder_t *builder, charon_node_kind_t kind) {
    assert(builder->current != NULL);
    builder->current = open_node_make(builder->current, kind);
}

void charon_builder_node_end(charon_builder_t *builder) {
    open_node_t *node = builder->current;
    assert(node != NULL);
    builder->current = node->parent;
    open_node_add_child(builder->current, open_node_build(builder->cache, node));
}

void charon_builder_token(charon_builder_t *builder, charon_token_kind_t kind, const char *text) {
    assert(builder->current != NULL);
    open_node_add_child(builder->current, charon_element_inner_make_token(builder->cache, kind, text));
}

charon_element_t *charon_builder_finish(charon_memory_allocator_t *allocator, charon_builder_t *builder) {
    assert(builder->current != NULL);
    assert(builder->current->parent == NULL);
    return charon_element_root(allocator, open_node_build(builder->cache, builder->current));
}
