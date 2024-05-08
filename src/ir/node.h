#pragma once
#include <stdint.h>
#include "type.h"
#include "../diag.h"

typedef enum {
    IR_NODE_TYPE_PROGRAM,
    IR_NODE_TYPE_FUNCTION,

    IR_NODE_TYPE_EXPR_LITERAL_NUMERIC,
    IR_NODE_TYPE_EXPR_LITERAL_STRING,
    IR_NODE_TYPE_EXPR_LITERAL_CHAR,
    IR_NODE_TYPE_EXPR_BINARY,
    IR_NODE_TYPE_EXPR_UNARY,
    IR_NODE_TYPE_EXPR_VAR,
    IR_NODE_TYPE_EXPR_CALL,
    IR_NODE_TYPE_EXPR_CAST,

    IR_NODE_TYPE_STMT_BLOCK,
    IR_NODE_TYPE_STMT_RETURN,
    IR_NODE_TYPE_STMT_IF,
    IR_NODE_TYPE_STMT_DECL
} ir_node_type_t;

typedef enum {
    IR_BINARY_OPERATION_ADDITION,
    IR_BINARY_OPERATION_SUBTRACTION,
    IR_BINARY_OPERATION_MULTIPLICATION,
    IR_BINARY_OPERATION_DIVISION,
    IR_BINARY_OPERATION_MODULO,
    IR_BINARY_OPERATION_GREATER,
    IR_BINARY_OPERATION_GREATER_EQUAL,
    IR_BINARY_OPERATION_LESS,
    IR_BINARY_OPERATION_LESS_EQUAL,
    IR_BINARY_OPERATION_EQUAL,
    IR_BINARY_OPERATION_NOT_EQUAL,
    IR_BINARY_OPERATION_ASSIGN
} ir_binary_operation_t;

typedef enum {
    IR_UNARY_OPERATION_NOT,
    IR_UNARY_OPERATION_NEGATIVE
} ir_unary_operation_t;

typedef struct {
    ir_type_t *type;
    const char *name;
} ir_function_argument_t;

typedef struct ir_node {
    ir_node_type_t type;
    diag_loc_t diag_loc;
    union {
        struct {
            size_t function_count;
            struct ir_node **functions;
        } program;

        struct {
            ir_type_t *type;
            const char *name;
            size_t argument_count;
            ir_function_argument_t *arguments;
            struct ir_node *body;
        } function;

        union {
            uintmax_t numeric_value;
            const char *string_value;
            char char_value;
        } expr_literal;
        struct {
            ir_binary_operation_t operation;
            struct ir_node *left, *right;
        } expr_binary;
        struct {
            ir_unary_operation_t operation;
            struct ir_node *operand;
        } expr_unary;
        struct {
            const char *name;
        } expr_var;
        struct {
            const char *name;
            size_t argument_count;
            struct ir_node **arguments;
        } expr_call;
        struct {
            struct ir_node *value;
            ir_type_t *type;
        } expr_cast;

        struct {
            size_t statement_count;
            struct ir_node **statements;
        } stmt_block;
        struct {
            struct ir_node *value; // OPTIONAL
        } stmt_return;
        struct {
            struct ir_node *condition;
            struct ir_node *body;
            struct ir_node *else_body; // OPTIONAL
        } stmt_if;
        struct {
            ir_type_t *type;
            const char *name;
            struct ir_node *initial; // OPTIONAL
        } stmt_decl;
    };
} ir_node_t;

ir_node_t *ir_node_make_program(size_t function_count, ir_node_t **functions, diag_loc_t diag_loc);
ir_node_t *ir_node_make_function(ir_type_t *type, const char *name, size_t argument_count, ir_function_argument_t *arguments, ir_node_t *body, diag_loc_t diag_loc);

ir_node_t *ir_node_make_expr_literal_numeric(uintmax_t value, diag_loc_t diag_loc);
ir_node_t *ir_node_make_expr_literal_string(const char *value, diag_loc_t diag_loc);
ir_node_t *ir_node_make_expr_literal_char(char value, diag_loc_t diag_loc);
ir_node_t *ir_node_make_expr_binary(ir_binary_operation_t operation, ir_node_t *left, ir_node_t *right, diag_loc_t diag_loc);
ir_node_t *ir_node_make_expr_unary(ir_unary_operation_t operation, ir_node_t *operand, diag_loc_t diag_loc);
ir_node_t *ir_node_make_expr_var(const char *name, diag_loc_t diag_loc);
ir_node_t *ir_node_make_expr_call(const char *name, size_t argument_count, ir_node_t **arguments, diag_loc_t diag_loc);
ir_node_t *ir_node_make_expr_cast(ir_node_t *value, ir_type_t *type, diag_loc_t diag_loc);

ir_node_t *ir_node_make_stmt_block(size_t statement_count, ir_node_t **statements, diag_loc_t diag_loc);
ir_node_t *ir_node_make_stmt_return(ir_node_t *value, diag_loc_t diag_loc);
ir_node_t *ir_node_make_stmt_if(ir_node_t *condition, ir_node_t *body, ir_node_t *else_body, diag_loc_t diag_loc);
ir_node_t *ir_node_make_stmt_decl(ir_type_t *type, const char *name, ir_node_t *initial, diag_loc_t diag_loc);