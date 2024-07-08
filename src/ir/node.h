#pragma once

#include "lib/source.h"

#include <stdint.h>

#define IR_NODE_LIST_INIT ((ir_node_list_t) { .count = 0, .first = NULL, .last = NULL })

typedef enum {
    IR_NODE_TYPE_ROOT,

    IR_NODE_TYPE_TLC_FUNCTION,

    IR_NODE_TYPE_STMT_BLOCK,
    IR_NODE_TYPE_STMT_EXPRESSION,

    IR_NODE_TYPE_EXPR_LITERAL_NUMERIC,
    IR_NODE_TYPE_EXPR_LITERAL_STRING,
    IR_NODE_TYPE_EXPR_LITERAL_CHAR,
    IR_NODE_TYPE_EXPR_LITERAL_BOOL,
    IR_NODE_TYPE_EXPR_BINARY,
    IR_NODE_TYPE_EXPR_UNARY
} ir_node_type_t;

typedef enum {
    IR_NODE_BINARY_OPERATION_ADDITION,
    IR_NODE_BINARY_OPERATION_SUBTRACTION,
    IR_NODE_BINARY_OPERATION_MULTIPLICATION,
    IR_NODE_BINARY_OPERATION_DIVISION,
    IR_NODE_BINARY_OPERATION_MODULO,
    IR_NODE_BINARY_OPERATION_GREATER,
    IR_NODE_BINARY_OPERATION_GREATER_EQUAL,
    IR_NODE_BINARY_OPERATION_LESS,
    IR_NODE_BINARY_OPERATION_LESS_EQUAL,
    IR_NODE_BINARY_OPERATION_EQUAL,
    IR_NODE_BINARY_OPERATION_NOT_EQUAL
} ir_node_binary_operation_t;

typedef enum {
    IR_NODE_UNARY_OPERATION_NOT,
    IR_NODE_UNARY_OPERATION_NEGATIVE,
    IR_NODE_UNARY_OPERATION_DEREF,
    IR_NODE_UNARY_OPERATION_REF
} ir_node_unary_operation_t;

typedef struct ir_node ir_node_t;

typedef struct {
    size_t count;
    ir_node_t *first, *last;
} ir_node_list_t;

struct ir_node {
    ir_node_type_t type;
    source_location_t source_location;
    union {
        struct {
            ir_node_list_t tlc_nodes;
        } root;

        struct {
            const char *name;
            ir_node_list_t statements;
        } tlc_function;

        struct {
            ir_node_list_t statements;
        } stmt_block;
        struct {
            ir_node_t *expression;
        } stmt_expression;

        union {
            uintmax_t numeric_value;
            const char *string_value;
            char char_value;
            bool bool_value;
        } expr_literal;
        struct {
            ir_node_binary_operation_t operation;
            ir_node_t *left, *right;
        } expr_binary;
        struct {
            ir_node_unary_operation_t operation;
            ir_node_t *operand;
        } expr_unary;
    };
    struct ir_node *next;
};

void ir_node_list_append(ir_node_list_t *list, ir_node_t *node);

ir_node_t *ir_node_make_root(ir_node_list_t tlc_nodes, source_location_t source_location);

ir_node_t *ir_node_make_tlc_function(const char *name, ir_node_list_t statements, source_location_t source_location);

ir_node_t *ir_node_make_stmt_block(ir_node_list_t statements, source_location_t source_location);
ir_node_t *ir_node_make_stmt_expression(ir_node_t *expression, source_location_t source_location);

ir_node_t *ir_node_make_expr_literal_numeric(uintmax_t value, source_location_t source_location);
ir_node_t *ir_node_make_expr_literal_string(const char *value, source_location_t source_location);
ir_node_t *ir_node_make_expr_literal_char(char value, source_location_t source_location);
ir_node_t *ir_node_make_expr_literal_bool(bool value, source_location_t source_location);
ir_node_t *ir_node_make_expr_binary(ir_node_binary_operation_t operation, ir_node_t *left, ir_node_t *right, source_location_t source_location);
ir_node_t *ir_node_make_expr_unary(ir_node_unary_operation_t operation, ir_node_t *operand, source_location_t source_location);