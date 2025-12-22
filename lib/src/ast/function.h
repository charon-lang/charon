#pragma once

#include "charon/element.h"

typedef struct {
    const charon_element_inner_t *inner_element;
} ast_node_function_t;

charon_utf8_text_t *ast_node_function_name(ast_node_function_t function);
const charon_element_inner_t *ast_node_function_type(ast_node_function_t function);
const charon_element_inner_t *ast_node_function_body(ast_node_function_t function);
