#include "module.h"

#include "charon/element.h"
#include "charon/node.h"
#include "charon/utf8.h"
#include "common/utf8.h"

#include <assert.h>

#define NAME_IDENTIFIER_INDEX 1

charon_utf8_text_t *ast_node_module_name(ast_node_module_t module) {
    assert(charon_element_type(module.inner_element) == CHARON_ELEMENT_TYPE_NODE);
    assert(charon_element_node_kind(module.inner_element) == CHARON_NODE_KIND_TLC_MODULE);

    const charon_element_inner_t *name_element = charon_element_node_child(module.inner_element, NAME_IDENTIFIER_INDEX);
    if(charon_element_type(name_element) != CHARON_ELEMENT_TYPE_TOKEN || charon_element_token_kind(name_element) != CHARON_TOKEN_KIND_IDENTIFIER) {
        assert(charon_element_type(name_element) == CHARON_ELEMENT_TYPE_NODE && charon_element_node_kind(name_element) == CHARON_NODE_KIND_ERROR);
        return nullptr;
    }

    return utf8_copy(charon_element_token_text(name_element));
}
