#pragma once

#include "charon/element.h"

typedef struct charon_builder charon_builder_t;

charon_builder_t *charon_builder_init(charon_element_cache_t *cache, charon_node_kind_t root_kind);
void charon_builder_node_start(charon_builder_t *builder, charon_node_kind_t kind);
void charon_builder_node_end(charon_builder_t *builder);
void charon_builder_token(charon_builder_t *builder, charon_token_kind_t kind, const char *text);
charon_element_t *charon_builder_finish(charon_memory_allocator_t *allocator, charon_builder_t *builder);
