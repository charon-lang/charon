#pragma once

#include "lib/memory.h"
#include "hlir/node.h"
#include "ir/ir.h"

typedef struct lower_context lower_context_t;

lower_context_t *lower_context_make(memory_allocator_t *work_allocator, ir_unit_t *unit, ir_type_cache_t *type_cache);

void lower_populate_namespace(lower_context_t *context, hlir_node_t *root_node);

void lower_nodes(lower_context_t *context, hlir_node_t *root_node);