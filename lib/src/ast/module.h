#pragma once

#include "charon/element.h"

typedef struct {
    const charon_element_inner_t *inner_element;
} ast_node_module_t;

charon_utf8_text_t *ast_node_module_name(ast_node_module_t module);
