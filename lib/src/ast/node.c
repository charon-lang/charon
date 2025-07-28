#include "node.h"

#include "lib/alloc.h"

static ast_node_t *node_make(ast_node_type_t type, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = alloc(sizeof(ast_node_t));
    node->type = type;
    node->source_location = source_location;
    node->attributes = attributes;
    node->next = NULL;
    return node;
}

size_t ast_node_list_count(ast_node_list_t *list) {
    return list->count;
}

void ast_node_list_append(ast_node_list_t *list, ast_node_t *node) {
    if(list->first == NULL) list->first = node;
    if(list->last != NULL) list->last->next = node;
    list->last = node;
    list->count++;
}

ast_node_t *ast_node_make_root(ast_node_list_t tlcs, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_ROOT, attributes, source_location);
    node->root.tlcs = tlcs;
    return node;
}

ast_node_t *ast_node_make_tlc_module(const char *name, ast_node_list_t tlcs, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_TLC_MODULE, attributes, source_location);
    node->tlc_module.name = name;
    node->tlc_module.tlcs = tlcs;
    return node;
}

ast_node_t *ast_node_make_tlc_function(
    const char *name,
    ast_type_function_t *function_type,
    const char **argument_names,
    ast_node_t *statement,
    size_t generic_parameter_count,
    const char **generic_parameters,
    ast_attribute_list_t attributes,
    source_location_t source_location
) {
    ast_node_t *node = node_make(AST_NODE_TYPE_TLC_FUNCTION, attributes, source_location);
    node->tlc_function.name = name;
    node->tlc_function.function_type = function_type;
    node->tlc_function.argument_names = argument_names;
    node->tlc_function.statement = statement;
    node->tlc_function.generic_parameter_count = generic_parameter_count;
    node->tlc_function.generic_parameters = generic_parameters;
    return node;
}

ast_node_t *ast_node_make_tlc_extern(const char *name, ast_type_function_t *function_type, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_TLC_EXTERN, attributes, source_location);
    node->tlc_extern.name = name;
    node->tlc_extern.function_type = function_type;
    return node;
}

ast_node_t *ast_node_make_tlc_type_definition(const char *name, ast_type_t *type, size_t generic_parameter_count, const char **generic_parameters, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_TLC_TYPE_DEFINITION, attributes, source_location);
    node->tlc_type_definition.name = name;
    node->tlc_type_definition.type = type;
    node->tlc_type_definition.generic_parameter_count = generic_parameter_count;
    node->tlc_type_definition.generic_parameters = generic_parameters;
    return node;
}

ast_node_t *ast_node_make_tlc_declaration(const char *name, ast_type_t *type, ast_node_t *initial, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_TLC_DECLARATION, attributes, source_location);
    node->tlc_declaration.name = name;
    node->tlc_declaration.type = type;
    node->tlc_declaration.initial = initial;
    return node;
}

ast_node_t *ast_node_make_tlc_enumeration(const char *name, size_t member_count, const char **members, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_TLC_ENUMERATION, attributes, source_location);
    node->tlc_enumeration.name = name;
    node->tlc_enumeration.member_count = member_count;
    node->tlc_enumeration.members = members;
    return node;
}

ast_node_t *ast_node_make_stmt_block(ast_node_list_t statements, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_STMT_BLOCK, attributes, source_location);
    node->stmt_block.statements = statements;
    return node;
}

ast_node_t *ast_node_make_stmt_declaration(const char *name, ast_type_t *type, ast_node_t *initial, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_STMT_DECLARATION, attributes, source_location);
    node->stmt_declaration.name = name;
    node->stmt_declaration.type = type;
    node->stmt_declaration.initial = initial;
    return node;
}

ast_node_t *ast_node_make_stmt_expression(ast_node_t *expression, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_STMT_EXPRESSION, attributes, source_location);
    node->stmt_expression.expression = expression;
    return node;
}

ast_node_t *ast_node_make_stmt_return(ast_node_t *value, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_STMT_RETURN, attributes, source_location);
    node->stmt_return.value = value;
    return node;
}

ast_node_t *ast_node_make_stmt_if(ast_node_t *condition, ast_node_t *body, ast_node_t *else_body, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_STMT_IF, attributes, source_location);
    node->stmt_if.condition = condition;
    node->stmt_if.body = body;
    node->stmt_if.else_body = else_body;
    return node;
}

ast_node_t *ast_node_make_stmt_while(ast_node_t *condition, ast_node_t *body, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_STMT_WHILE, attributes, source_location);
    node->stmt_while.condition = condition;
    node->stmt_while.body = body;
    return node;
}

ast_node_t *ast_node_make_stmt_continue(ast_attribute_list_t attributes, source_location_t source_location) {
    return node_make(AST_NODE_TYPE_STMT_CONTINUE, attributes, source_location);
}

ast_node_t *ast_node_make_stmt_break(ast_attribute_list_t attributes, source_location_t source_location) {
    return node_make(AST_NODE_TYPE_STMT_BREAK, attributes, source_location);
}

ast_node_t *ast_node_make_stmt_for(ast_node_t *declaration, ast_node_t *condition, ast_node_t *expr_after, ast_node_t *body, ast_attribute_list_t attributes, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_STMT_FOR, attributes, source_location);
    node->stmt_for.declaration = declaration;
    node->stmt_for.condition = condition;
    node->stmt_for.expr_after = expr_after;
    node->stmt_for.body = body;
    return node;
}

ast_node_t *ast_node_make_expr_literal_numeric(uintmax_t value, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_LITERAL_NUMERIC, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_literal.numeric_value = value;
    return node;
}

ast_node_t *ast_node_make_expr_literal_string(const char *value, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_LITERAL_STRING, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_literal.string_value = value;
    return node;
}

ast_node_t *ast_node_make_expr_literal_char(char value, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_LITERAL_CHAR, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_literal.char_value = value;
    return node;
}

ast_node_t *ast_node_make_expr_literal_bool(bool value, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_LITERAL_BOOL, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_literal.bool_value = value;
    return node;
}

ast_node_t *ast_node_make_expr_binary(ast_node_binary_operation_t operation, ast_node_t *left, ast_node_t *right, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_BINARY, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_binary.operation = operation;
    node->expr_binary.left = left;
    node->expr_binary.right = right;
    return node;
}

ast_node_t *ast_node_make_expr_unary(ast_node_unary_operation_t operation, ast_node_t *operand, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_UNARY, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_unary.operation = operation;
    node->expr_unary.operand = operand;
    return node;
}

ast_node_t *ast_node_make_expr_variable(const char *name, size_t generic_parameter_count, ast_type_t **generic_parameters, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_VARIABLE, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_variable.generic_parameter_count = generic_parameter_count;
    node->expr_variable.generic_parameters = generic_parameters;
    node->expr_variable.name = name;
    return node;
}

ast_node_t *ast_node_make_expr_call(ast_node_t *function_reference, ast_node_list_t arguments, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_CALL, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_call.function_reference = function_reference;
    node->expr_call.arguments = arguments;
    return node;
}

ast_node_t *ast_node_make_expr_tuple(ast_node_list_t values, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_TUPLE, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_tuple.values = values;
    return node;
}

ast_node_t *ast_node_make_expr_cast(ast_node_t *value, ast_type_t *type, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_CAST, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_cast.value = value;
    node->expr_cast.type = type;
    return node;
}

ast_node_t *ast_node_make_expr_subscript_index(ast_node_t *value, ast_node_t *index, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_SUBSCRIPT, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_subscript.type = AST_NODE_SUBSCRIPT_TYPE_INDEX;
    node->expr_subscript.value = value;
    node->expr_subscript.index = index;
    return node;
}

ast_node_t *ast_node_make_expr_subscript_index_const(ast_node_t *value, uintmax_t index, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_SUBSCRIPT, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_subscript.type = AST_NODE_SUBSCRIPT_TYPE_INDEX_CONST;
    node->expr_subscript.value = value;
    node->expr_subscript.index_const = index;
    return node;
}

ast_node_t *ast_node_make_expr_subscript_member(ast_node_t *value, const char *name, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_SUBSCRIPT, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_subscript.type = AST_NODE_SUBSCRIPT_TYPE_MEMBER;
    node->expr_subscript.value = value;
    node->expr_subscript.member = name;
    return node;
}

ast_node_t *ast_node_make_expr_selector(const char *name, ast_node_t *value, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_SELECTOR, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_selector.name = name;
    node->expr_selector.value = value;
    return node;
}

ast_node_t *ast_node_make_expr_sizeof(ast_type_t *type, source_location_t source_location) {
    ast_node_t *node = node_make(AST_NODE_TYPE_EXPR_SIZEOF, AST_ATTRIBUTE_LIST_INIT, source_location);
    node->expr_sizeof.type = type;
    return node;
}
