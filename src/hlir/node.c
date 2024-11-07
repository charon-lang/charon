#include "node.h"

#include <stdlib.h>

static hlir_node_t *node_make(hlir_node_type_t type, source_location_t source_location) {
    hlir_node_t *node = malloc(sizeof(hlir_node_t));
    node->type = type;
    node->source_location = source_location;
    node->next = NULL;
    return node;
}

size_t hlir_node_list_count(hlir_node_list_t *list) {
    return list->count;
}

void hlir_node_list_append(hlir_node_list_t *list, hlir_node_t *node) {
    if(list->first == NULL) list->first = node;
    if(list->last != NULL) list->last->next = node;
    list->last = node;
    list->count++;
}

hlir_node_t *hlir_node_make_root(hlir_node_list_t tlcs, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_ROOT, source_location);
    node->root.tlcs = tlcs;
    return node;
}

hlir_node_t *hlir_node_make_tlc_module(const char *name, hlir_node_list_t tlcs, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_TLC_MODULE, source_location);
    node->tlc_module.name = name;
    node->tlc_module.tlcs = tlcs;
    return node;
}

hlir_node_t *hlir_node_make_tlc_function(const char *name, hlir_type_t *type, const char **argument_names, hlir_node_t *statement, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_TLC_FUNCTION, source_location);
    node->tlc_function.name = name;
    node->tlc_function.type = type;
    node->tlc_function.argument_names = argument_names;
    node->tlc_function.statement = statement;
    return node;
}

hlir_node_t *hlir_node_make_tlc_extern(const char *name, hlir_type_t *type, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_TLC_EXTERN, source_location);
    node->tlc_function.name = name;
    node->tlc_extern.type = type;
    return node;
}

hlir_node_t *hlir_node_make_tlc_type_definition(const char *name, hlir_type_t *type, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_TLC_TYPE_DEFINITION, source_location);
    node->tlc_type_definition.name = name;
    node->tlc_type_definition.type = type;
    return node;
}

hlir_node_t *hlir_node_make_stmt_block(hlir_node_list_t statements, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_STMT_BLOCK, source_location);
    node->stmt_block.statements = statements;
    return node;
}

hlir_node_t *hlir_node_make_stmt_declaration(const char *name, hlir_type_t *type, hlir_node_t *initial, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_STMT_DECLARATION, source_location);
    node->stmt_declaration.name = name;
    node->stmt_declaration.type = type;
    node->stmt_declaration.initial = initial;
    return node;
}

hlir_node_t *hlir_node_make_stmt_expression(hlir_node_t *expression, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_STMT_EXPRESSION, source_location);
    node->stmt_expression.expression = expression;
    return node;
}

hlir_node_t *hlir_node_make_stmt_return(hlir_node_t *value, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_STMT_RETURN, source_location);
    node->stmt_return.value = value;
    return node;
}

hlir_node_t *hlir_node_make_stmt_if(hlir_node_t *condition, hlir_node_t *body, hlir_node_t *else_body, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_STMT_IF, source_location);
    node->stmt_if.condition = condition;
    node->stmt_if.body = body;
    node->stmt_if.else_body = else_body;
    return node;
}

hlir_node_t *hlir_node_make_stmt_while(hlir_node_t *condition, hlir_node_t *body, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_STMT_WHILE, source_location);
    node->stmt_while.condition = condition;
    node->stmt_while.body = body;
    return node;
}

hlir_node_t *hlir_node_make_expr_literal_numeric(uintmax_t value, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC, source_location);
    node->expr_literal.numeric_value = value;
    return node;
}

hlir_node_t *hlir_node_make_expr_literal_string(const char *value, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_LITERAL_STRING, source_location);
    node->expr_literal.string_value = value;
    return node;
}

hlir_node_t *hlir_node_make_expr_literal_char(char value, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_LITERAL_CHAR, source_location);
    node->expr_literal.char_value = value;
    return node;
}

hlir_node_t *hlir_node_make_expr_literal_bool(bool value, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_LITERAL_BOOL, source_location);
    node->expr_literal.bool_value = value;
    return node;
}

hlir_node_t *hlir_node_make_expr_binary(hlir_node_binary_operation_t operation, hlir_node_t *left, hlir_node_t *right, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_BINARY, source_location);
    node->expr_binary.operation = operation;
    node->expr_binary.left = left;
    node->expr_binary.right = right;
    return node;
}

hlir_node_t *hlir_node_make_expr_unary(hlir_node_unary_operation_t operation, hlir_node_t *operand, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_UNARY, source_location);
    node->expr_unary.operation = operation;
    node->expr_unary.operand = operand;
    return node;
}

hlir_node_t *hlir_node_make_expr_variable(const char *name, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_VARIABLE, source_location);
    node->expr_variable.name = name;
    return node;
}

hlir_node_t *hlir_node_make_expr_call(const char *function_name, hlir_node_list_t arguments, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_CALL, source_location);
    node->expr_call.function_name = function_name;
    node->expr_call.arguments = arguments;
    return node;
}

hlir_node_t *hlir_node_make_expr_tuple(hlir_node_list_t values, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_TUPLE, source_location);
    node->expr_tuple.values = values;
    return node;
}

hlir_node_t *hlir_node_make_expr_cast(hlir_node_t *value, hlir_type_t *type, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_CAST, source_location);
    node->expr_cast.value = value;
    node->expr_cast.type = type;
    return node;
}

hlir_node_t *hlir_node_make_expr_subscript_index(hlir_node_t *value, hlir_node_t *index, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_SUBSCRIPT, source_location);
    node->expr_subscript.type = HLIR_NODE_SUBSCRIPT_TYPE_INDEX;
    node->expr_subscript.value = value;
    node->expr_subscript.index = index;
    return node;
}

hlir_node_t *hlir_node_make_expr_subscript_index_const(hlir_node_t *value, uintmax_t index, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_SUBSCRIPT, source_location);
    node->expr_subscript.type = HLIR_NODE_SUBSCRIPT_TYPE_INDEX_CONST;
    node->expr_subscript.value = value;
    node->expr_subscript.index_const = index;
    return node;
}

hlir_node_t *hlir_node_make_expr_subscript_member(hlir_node_t *value, const char *name, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_SUBSCRIPT, source_location);
    node->expr_subscript.type = HLIR_NODE_SUBSCRIPT_TYPE_MEMBER;
    node->expr_subscript.value = value;
    node->expr_subscript.member = name;
    return node;
}

hlir_node_t *hlir_node_make_expr_selector(const char *name, hlir_node_t *value, source_location_t source_location) {
    hlir_node_t *node = node_make(HLIR_NODE_TYPE_EXPR_SELECTOR, source_location);
    node->expr_selector.name = name;
    node->expr_selector.value = value;
    return node;
}