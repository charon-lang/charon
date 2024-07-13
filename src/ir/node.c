#include "node.h"

#include <stdlib.h>

static ir_node_t *make_node(ir_node_type_t type, source_location_t source_location) {
    ir_node_t *node = malloc(sizeof(ir_node_t));
    node->type = type;
    node->source_location = source_location;
    node->next = NULL;
    return node;
}

void ir_node_list_append(ir_node_list_t *list, ir_node_t *node) {
    if(list->first == NULL) list->first = node;
    if(list->last != NULL) list->last->next = node;
    list->last = node;
    list->count++;
}

size_t ir_node_list_count(ir_node_list_t *list) {
    return list->count;
}

ir_node_t *ir_node_make_root(ir_node_list_t tlc_nodes, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_ROOT, source_location);
    node->root.tlc_nodes = tlc_nodes;
    return node;
}

ir_node_t *ir_node_make_tlc_function(const char *name, ir_node_list_t statements, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_TLC_FUNCTION, source_location);
    node->tlc_function.name = name;
    node->tlc_function.statements = statements;
    return node;
}

ir_node_t *ir_node_make_stmt_block(ir_node_list_t statements, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_STMT_BLOCK, source_location);
    node->stmt_block.statements = statements;
    return node;
}

ir_node_t *ir_node_make_stmt_declaration(const char *name, ir_type_t *type, ir_node_t *initial, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_STMT_DECLARATION, source_location);
    node->stmt_declaration.name = name;
    node->stmt_declaration.type = type;
    node->stmt_declaration.initial = initial;
    return node;
}

ir_node_t *ir_node_make_stmt_expression(ir_node_t *expression, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_STMT_EXPRESSION, source_location);
    node->stmt_expression.expression = expression;
    return node;
}

ir_node_t *ir_node_make_expr_literal_numeric(uintmax_t value, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_EXPR_LITERAL_NUMERIC, source_location);
    node->expr_literal.numeric_value = value;
    return node;
}

ir_node_t *ir_node_make_expr_literal_string(const char *value, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_EXPR_LITERAL_STRING, source_location);
    node->expr_literal.string_value = value;
    return node;
}

ir_node_t *ir_node_make_expr_literal_char(char value, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_EXPR_LITERAL_CHAR, source_location);
    node->expr_literal.char_value = value;
    return node;
}

ir_node_t *ir_node_make_expr_literal_bool(bool value, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_EXPR_LITERAL_BOOL, source_location);
    node->expr_literal.bool_value = value;
    return node;
}

ir_node_t *ir_node_make_expr_binary(ir_node_binary_operation_t operation, ir_node_t *left, ir_node_t *right, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_EXPR_BINARY, source_location);
    node->expr_binary.operation = operation;
    node->expr_binary.left = left;
    node->expr_binary.right = right;
    return node;
}

ir_node_t *ir_node_make_expr_unary(ir_node_unary_operation_t operation, ir_node_t *operand, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_EXPR_UNARY, source_location);
    node->expr_unary.operation = operation;
    node->expr_unary.operand = operand;
    return node;
}

ir_node_t *ir_node_make_expr_variable(const char *name, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_EXPR_VARIABLE, source_location);
    node->expr_variable.name = name;
    return node;
}

ir_node_t *ir_node_make_expr_call(const char *function_name, ir_node_list_t arguments, source_location_t source_location) {
    ir_node_t *node = make_node(IR_NODE_TYPE_EXPR_CALL, source_location);
    node->expr_call.function_name = function_name;
    node->expr_call.arguments = arguments;
    return node;
}