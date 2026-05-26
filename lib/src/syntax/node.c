#include "charon/syntax/node.h"

#include <assert.h>

const char *charon_node_kind_tostring(charon_node_kind_t kind) {
    static const char *translations[] = {
#define NODE(ID, NAME) [CHARON_NODE_KIND_##ID] = NAME,
#include "charon/syntax/node.def"
#undef NODE
    };
    return translations[kind];
}
