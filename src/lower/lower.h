#pragma once

#include "lib/memory.h"
#include "hlir/node.h"
#include "llir/node.h"
#include "llir/namespace.h"

typedef struct lower_context lower_context_t;

lower_context_t *lower_context_make(memory_allocator_t *work_allocator, llir_namespace_t *root_namespace, llir_type_cache_t *anon_type_cache);

void lower_populate_namespace(lower_context_t *context, hlir_node_t *root_node);

llir_node_t *lower_nodes(lower_context_t *context, hlir_node_t *root_node);