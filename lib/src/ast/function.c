#include "function.h"

#include "charon/element.h"
#include "charon/node.h"
#include "charon/utf8.h"
#include "common/utf8.h"

#include <assert.h>
#include <stddef.h>

#define NAME_IDENTIFIER_INDEX 1
#define FIRST_POSSIBLE_TYPE_INDEX 2
#define FIRST_POSSIBLE_BODY_INDEX 3

charon_utf8_text_t *ast_node_function_name(ast_node_function_t function) {
    assert(charon_element_type(function.inner_element) == CHARON_ELEMENT_TYPE_NODE);
    assert(charon_element_node_kind(function.inner_element) == CHARON_NODE_KIND_TLC_FUNCTION);

    const charon_element_inner_t *name_element = charon_element_node_child(function.inner_element, NAME_IDENTIFIER_INDEX);
    if(charon_element_type(name_element) != CHARON_ELEMENT_TYPE_TOKEN || charon_element_token_kind(name_element) != CHARON_TOKEN_KIND_IDENTIFIER) {
        assert(charon_element_type(name_element) == CHARON_ELEMENT_TYPE_NODE && charon_element_node_kind(name_element) == CHARON_NODE_KIND_ERROR);
        return nullptr;
    }

    return utf8_copy(charon_element_token_text(name_element));
}

const charon_element_inner_t *ast_node_function_type(ast_node_function_t function) {
    assert(charon_element_type(function.inner_element) == CHARON_ELEMENT_TYPE_NODE);
    assert(charon_element_node_kind(function.inner_element) == CHARON_NODE_KIND_TLC_FUNCTION);

    size_t child_count = charon_element_node_child_count(function.inner_element);
    for(size_t i = FIRST_POSSIBLE_TYPE_INDEX; i < child_count; i++) {
        const charon_element_inner_t *child = charon_element_node_child(function.inner_element, i);
        if(charon_element_type(child) != CHARON_ELEMENT_TYPE_NODE || charon_element_node_kind(child) != CHARON_NODE_KIND_TYPE_FUNCTION) continue;
        return child;
    }
    return nullptr;
}

const charon_element_inner_t *ast_node_function_body(ast_node_function_t function) {
    assert(charon_element_type(function.inner_element) == CHARON_ELEMENT_TYPE_NODE);
    assert(charon_element_node_kind(function.inner_element) == CHARON_NODE_KIND_TLC_FUNCTION);

    size_t child_count = charon_element_node_child_count(function.inner_element);
    for(size_t i = FIRST_POSSIBLE_BODY_INDEX; i < child_count; i++) {
        const charon_element_inner_t *child = charon_element_node_child(function.inner_element, i);
        if(charon_element_type(child) != CHARON_ELEMENT_TYPE_NODE || charon_element_node_kind(child) != CHARON_NODE_KIND_STMT) continue;
        return child;
    }
    return nullptr;
}
