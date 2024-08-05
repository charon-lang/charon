#pragma once

#include "lib/source.h"
#include "ir/type.h"
#include "ir/function.h"

#include <stdint.h>

#define IR_NODE_LIST_INIT ((ir_node_list_t) { .count = 0, .first = NULL, .last = NULL })
#define IR_NODE_LIST_FOREACH(LIST, BODY) { ir_node_t *__node = (LIST)->first; for(size_t __i = 0; __i < ir_node_list_count(LIST); __i++) { assert(__node != NULL); { ir_node_t *node = __node; size_t i = __i; BODY; }; __node = __node->next; }; assert(__node == NULL); }

typedef enum {
    IR_NODE_TYPE_ROOT,

    IR_NODE_TYPE_TLC_FUNCTION,

    IR_NODE_TYPE_STMT_BLOCK,
    IR_NODE_TYPE_STMT_DECLARATION,
    IR_NODE_TYPE_STMT_EXPRESSION,
    IR_NODE_TYPE_STMT_RETURN,
    IR_NODE_TYPE_STMT_IF,
    IR_NODE_TYPE_STMT_WHILE,

    IR_NODE_TYPE_EXPR_LITERAL_NUMERIC,
    IR_NODE_TYPE_EXPR_LITERAL_STRING,
    IR_NODE_TYPE_EXPR_LITERAL_CHAR,
    IR_NODE_TYPE_EXPR_LITERAL_BOOL,
    IR_NODE_TYPE_EXPR_BINARY,
    IR_NODE_TYPE_EXPR_UNARY,
    IR_NODE_TYPE_EXPR_VARIABLE,
    IR_NODE_TYPE_EXPR_CALL,
    IR_NODE_TYPE_EXPR_TUPLE,
    IR_NODE_TYPE_EXPR_CAST,
    IR_NODE_TYPE_EXPR_ACCESS_INDEX,
    IR_NODE_TYPE_EXPR_ACCESS_INDEX_CONST
} ir_node_type_t;

typedef enum {
    IR_NODE_BINARY_OPERATION_ASSIGN,
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
            ir_function_t *prototype;
            ir_node_list_t statements;
        } tlc_function;

        struct {
            ir_node_list_t statements;
        } stmt_block;
        struct {
            const char *name;
            ir_type_t *type; // OPTIONAL
            ir_node_t *initial; // OPTIONAL
        } stmt_declaration;
        struct {
            ir_node_t *expression;
        } stmt_expression;
        struct {
            ir_node_t *value; // OPTIONAL
        } stmt_return;
        struct {
            ir_node_t *condition;
            ir_node_t *body;
            ir_node_t *else_body; // OPTIONAL
        } stmt_if;
        struct {
            ir_node_t *condition; // OPTIONAL
            ir_node_t *body;
        } stmt_while;

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
        struct {
            const char *name;
        } expr_variable;
        struct {
            const char *function_name;
            ir_node_list_t arguments;
        } expr_call;
        struct {
            ir_node_list_t values;
        } expr_tuple;
        struct {
            ir_node_t *value;
            ir_type_t *type;
        } expr_cast;
        struct {
            ir_node_t *value;
            ir_node_t *index;
        } expr_access_index;
        struct {
            ir_node_t *value;
            uintmax_t index;
        } expr_access_index_const;
    };
    struct ir_node *next;
};

void ir_node_list_append(ir_node_list_t *list, ir_node_t *node);
size_t ir_node_list_count(ir_node_list_t *list);

ir_node_t *ir_node_make_root(ir_node_list_t tlc_nodes, source_location_t source_location);

ir_node_t *ir_node_make_tlc_function(ir_function_t *prototype, ir_node_list_t statements, source_location_t source_location);

ir_node_t *ir_node_make_stmt_block(ir_node_list_t statements, source_location_t source_location);
ir_node_t *ir_node_make_stmt_declaration(const char *name, ir_type_t *type, ir_node_t *initial, source_location_t source_location);
ir_node_t *ir_node_make_stmt_expression(ir_node_t *expression, source_location_t source_location);
ir_node_t *ir_node_make_stmt_return(ir_node_t *value, source_location_t source_location);
ir_node_t *ir_node_make_stmt_if(ir_node_t *condition, ir_node_t *body, ir_node_t *else_body, source_location_t source_location);
ir_node_t *ir_node_make_stmt_while(ir_node_t *condition, ir_node_t *body, source_location_t source_location);

ir_node_t *ir_node_make_expr_literal_numeric(uintmax_t value, source_location_t source_location);
ir_node_t *ir_node_make_expr_literal_string(const char *value, source_location_t source_location);
ir_node_t *ir_node_make_expr_literal_char(char value, source_location_t source_location);
ir_node_t *ir_node_make_expr_literal_bool(bool value, source_location_t source_location);
ir_node_t *ir_node_make_expr_binary(ir_node_binary_operation_t operation, ir_node_t *left, ir_node_t *right, source_location_t source_location);
ir_node_t *ir_node_make_expr_unary(ir_node_unary_operation_t operation, ir_node_t *operand, source_location_t source_location);
ir_node_t *ir_node_make_expr_variable(const char *name, source_location_t source_location);
ir_node_t *ir_node_make_expr_call(const char *function_name, ir_node_list_t arguments, source_location_t source_location);
ir_node_t *ir_node_make_expr_tuple(ir_node_list_t values, source_location_t source_location);
ir_node_t *ir_node_make_expr_cast(ir_node_t *value, ir_type_t *type, source_location_t source_location);
ir_node_t *ir_node_make_expr_access_index(ir_node_t *value, ir_node_t *index, source_location_t source_location);
ir_node_t *ir_node_make_expr_access_index_const(ir_node_t *value, uintmax_t index, source_location_t source_location);