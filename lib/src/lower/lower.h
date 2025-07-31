#pragma once

#include "ast/node.h"
#include "ir/ir.h"

void lower_to_ir(memory_allocator_t *work_allocator, ir_unit_t *unit, ir_type_cache_t *type_cache, ast_node_list_t *root_nodes);
