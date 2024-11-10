#pragma once

#include "hlir/node.h"
#include "llir/namespace.h"
#include "llir/node.h"

llir_node_t *semantics(hlir_node_t *root_node, llir_namespace_t *root_namespace, llir_type_cache_t *anon_type_cache);