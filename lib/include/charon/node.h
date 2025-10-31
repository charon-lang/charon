#pragma once

#include <stddef.h>

typedef enum charon_node_kind {
#define NODE(NAME, ...) CHARON_NODE_KIND_##NAME,
#include "charon/nodes.def"
#undef NODE
} charon_node_kind_t;

const char *charon_node_kind_tostring(charon_node_kind_t kind);
