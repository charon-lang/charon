#include "charon/node.h"

#include <assert.h>

const char *node_kind_tostring(charon_node_kind_t kind) {
    static const char *translations[] = {
#define NODE(ID, NAME) [NODE_KIND_##ID] = NAME,
#include "charon/nodes.def"
#undef NODE
    };
    return translations[kind];
}
