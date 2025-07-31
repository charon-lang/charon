#pragma once

#include "ast/attribute.h"
#include "ast/type.h"
#include "lib/diag.h"
#include "lib/source.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* variables in body: `node`, `i` */
#define AST_NODE_LIST_FOREACH(LIST, BODY)                                                    \
    {                                                                                        \
        ast_node_t *__node = (LIST)->first;                                                  \
        for(size_t __i = 0; __i < ast_node_list_count(LIST); __i++, __node = __node->next) { \
            assert(__node != NULL);                                                          \
            {                                                                                \
                [[maybe_unused]] ast_node_t *node = __node;                                  \
                [[maybe_unused]] size_t i = __i;                                             \
                BODY;                                                                        \
            };                                                                               \
        };                                                                                   \
    }
#define AST_NODE_LIST_INIT ((ast_node_list_t) { .count = 0, .first = NULL, .last = NULL })

#define AST_CASE_TLC(BODY)                           \
    case AST_NODE_TYPE_TLC_MODULE:                   \
    case AST_NODE_TYPE_TLC_FUNCTION:                 \
    case AST_NODE_TYPE_TLC_EXTERN:                   \
    case AST_NODE_TYPE_TLC_TYPE_DEFINITION:          \
    case AST_NODE_TYPE_TLC_DECLARATION:              \
    case AST_NODE_TYPE_TLC_ENUMERATION:     BODY; break;

#define AST_CASE_STMT(BODY)                      \
    case AST_NODE_TYPE_STMT_BLOCK:               \
    case AST_NODE_TYPE_STMT_DECLARATION:         \
    case AST_NODE_TYPE_STMT_EXPRESSION:          \
    case AST_NODE_TYPE_STMT_RETURN:              \
    case AST_NODE_TYPE_STMT_IF:                  \
    case AST_NODE_TYPE_STMT_WHILE:               \
    case AST_NODE_TYPE_STMT_CONTINUE:            \
    case AST_NODE_TYPE_STMT_BREAK:               \
    case AST_NODE_TYPE_STMT_FOR:                 \
    case AST_NODE_TYPE_STMT_SWITCH:      BODY; break;

#define AST_CASE_EXPRESSION(BODY)                \
    case AST_NODE_TYPE_EXPR_LITERAL_NUMERIC:     \
    case AST_NODE_TYPE_EXPR_LITERAL_STRING:      \
    case AST_NODE_TYPE_EXPR_LITERAL_CHAR:        \
    case AST_NODE_TYPE_EXPR_LITERAL_BOOL:        \
    case AST_NODE_TYPE_EXPR_LITERAL_STRUCT:      \
    case AST_NODE_TYPE_EXPR_BINARY:              \
    case AST_NODE_TYPE_EXPR_UNARY:               \
    case AST_NODE_TYPE_EXPR_VARIABLE:            \
    case AST_NODE_TYPE_EXPR_TUPLE:               \
    case AST_NODE_TYPE_EXPR_CALL:                \
    case AST_NODE_TYPE_EXPR_CAST:                \
    case AST_NODE_TYPE_EXPR_SUBSCRIPT:           \
    case AST_NODE_TYPE_EXPR_SELECTOR:            \
    case AST_NODE_TYPE_EXPR_SIZEOF:          BODY; break;

typedef enum {
    AST_NODE_TYPE_ROOT,

    AST_NODE_TYPE_ERROR,

    AST_NODE_TYPE_TLC_MODULE,
    AST_NODE_TYPE_TLC_FUNCTION,
    AST_NODE_TYPE_TLC_EXTERN,
    AST_NODE_TYPE_TLC_TYPE_DEFINITION,
    AST_NODE_TYPE_TLC_DECLARATION,
    AST_NODE_TYPE_TLC_ENUMERATION,

    AST_NODE_TYPE_STMT_BLOCK,
    AST_NODE_TYPE_STMT_DECLARATION,
    AST_NODE_TYPE_STMT_EXPRESSION,
    AST_NODE_TYPE_STMT_RETURN,
    AST_NODE_TYPE_STMT_IF,
    AST_NODE_TYPE_STMT_WHILE,
    AST_NODE_TYPE_STMT_CONTINUE,
    AST_NODE_TYPE_STMT_BREAK,
    AST_NODE_TYPE_STMT_FOR,
    AST_NODE_TYPE_STMT_SWITCH,

    AST_NODE_TYPE_EXPR_LITERAL_NUMERIC,
    AST_NODE_TYPE_EXPR_LITERAL_STRING,
    AST_NODE_TYPE_EXPR_LITERAL_CHAR,
    AST_NODE_TYPE_EXPR_LITERAL_BOOL,
    AST_NODE_TYPE_EXPR_LITERAL_STRUCT,
    AST_NODE_TYPE_EXPR_BINARY,
    AST_NODE_TYPE_EXPR_UNARY,
    AST_NODE_TYPE_EXPR_VARIABLE,
    AST_NODE_TYPE_EXPR_CALL,
    AST_NODE_TYPE_EXPR_TUPLE,
    AST_NODE_TYPE_EXPR_CAST,
    AST_NODE_TYPE_EXPR_SUBSCRIPT,
    AST_NODE_TYPE_EXPR_SELECTOR,
    AST_NODE_TYPE_EXPR_SIZEOF
} ast_node_type_t;

typedef enum {
    AST_NODE_BINARY_OPERATION_ASSIGN,
    AST_NODE_BINARY_OPERATION_ADDITION,
    AST_NODE_BINARY_OPERATION_SUBTRACTION,
    AST_NODE_BINARY_OPERATION_MULTIPLICATION,
    AST_NODE_BINARY_OPERATION_DIVISION,
    AST_NODE_BINARY_OPERATION_MODULO,
    AST_NODE_BINARY_OPERATION_GREATER,
    AST_NODE_BINARY_OPERATION_GREATER_EQUAL,
    AST_NODE_BINARY_OPERATION_LESS,
    AST_NODE_BINARY_OPERATION_LESS_EQUAL,
    AST_NODE_BINARY_OPERATION_EQUAL,
    AST_NODE_BINARY_OPERATION_NOT_EQUAL,
    AST_NODE_BINARY_OPERATION_LOGICAL_AND,
    AST_NODE_BINARY_OPERATION_LOGICAL_OR,
    AST_NODE_BINARY_OPERATION_SHIFT_LEFT,
    AST_NODE_BINARY_OPERATION_SHIFT_RIGHT,
    AST_NODE_BINARY_OPERATION_AND,
    AST_NODE_BINARY_OPERATION_OR,
    AST_NODE_BINARY_OPERATION_XOR
} ast_node_binary_operation_t;

typedef enum {
    AST_NODE_UNARY_OPERATION_NOT,
    AST_NODE_UNARY_OPERATION_NEGATIVE,
    AST_NODE_UNARY_OPERATION_DEREF,
    AST_NODE_UNARY_OPERATION_REF
} ast_node_unary_operation_t;

typedef enum {
    AST_NODE_SUBSCRIPT_TYPE_INDEX,
    AST_NODE_SUBSCRIPT_TYPE_INDEX_CONST,
    AST_NODE_SUBSCRIPT_TYPE_MEMBER,
} ast_node_subscript_type_t;

typedef struct ast_node ast_node_t;

typedef struct {
    size_t count;
    ast_node_t *first, *last;
} ast_node_list_t;

typedef struct {
    const char *name;
    ast_node_t *value;
    source_location_t source_location;
} ast_node_struct_literal_member_t;

typedef struct {
    ast_node_t *value;
    ast_node_t *body;
} ast_node_switch_case_t;

struct ast_node {
    ast_node_type_t type;
    ast_attribute_list_t attributes;
    union {
        source_location_t source_location;
        /* associated_diagnostic is used when type is error */
        diag_t *associated_diagnostic;
    };
    union {
        struct {
            ast_node_list_t tlcs;
        } root;

        struct {
            const char *name;
            ast_node_list_t tlcs;
        } tlc_module;
        struct {
            const char *name;
            const char **argument_names;
            ast_type_function_t *function_type;
            ast_node_t *statement; /* nullable */
            size_t generic_parameter_count;
            const char **generic_parameters;
        } tlc_function;
        struct {
            const char *name;
            ast_type_function_t *function_type;
        } tlc_extern;
        struct {
            const char *name;
            ast_type_t *type;
            size_t generic_parameter_count;
            const char **generic_parameters;
        } tlc_type_definition;
        struct {
            const char *name;
            ast_type_t *type;
            ast_node_t *initial; /* nullable */
        } tlc_declaration;
        struct {
            const char *name;
            size_t member_count;
            const char **members;
        } tlc_enumeration;

        struct {
            ast_node_list_t statements;
        } stmt_block;
        struct {
            const char *name;
            ast_type_t *type; /* nullable */
            ast_node_t *initial; /* nullable */
        } stmt_declaration;
        struct {
            ast_node_t *expression;
        } stmt_expression;
        struct {
            ast_node_t *value; /* nullable */
        } stmt_return;
        struct {
            ast_node_t *condition;
            ast_node_t *body, *else_body; /* nullable */
        } stmt_if;
        struct {
            ast_node_t *condition; /* nullable */
            ast_node_t *body; /* nullable */
        } stmt_while;
        struct {
            ast_node_t *declaration, *condition, *expr_after; /* nullable */
            ast_node_t *body; /* nullable */
        } stmt_for;
        struct {
            ast_node_t *value;
            size_t case_count;
            ast_node_switch_case_t *cases;
            ast_node_t *default_body; /* nullable */
        } stmt_switch;

        union {
            uintmax_t numeric_value;
            const char *string_value;
            char char_value;
            bool bool_value;
            struct {
                const char *type_name;
                size_t member_count;
                ast_node_struct_literal_member_t *members;
            } struct_value;
        } expr_literal;
        struct {
            ast_node_binary_operation_t operation;
            ast_node_t *left, *right;
        } expr_binary;
        struct {
            ast_node_unary_operation_t operation;
            ast_node_t *operand;
        } expr_unary;
        struct {
            const char *name;
            size_t generic_parameter_count;
            ast_type_t **generic_parameters;
        } expr_variable;
        struct {
            ast_node_t *function_reference;
            ast_node_list_t arguments;
        } expr_call;
        struct {
            ast_node_list_t values;
        } expr_tuple;
        struct {
            ast_node_t *value;
            ast_type_t *type;
        } expr_cast;
        struct {
            ast_node_subscript_type_t type;
            ast_node_t *value;
            union {
                uintmax_t index_const;
                ast_node_t *index;
                const char *member;
            };
        } expr_subscript;
        struct {
            const char *name;
            ast_node_t *value;
        } expr_selector;
        struct {
            ast_type_t *type;
        } expr_sizeof;
    };
    ast_node_t *next;
};

size_t ast_node_list_count(ast_node_list_t *list);
void ast_node_list_append(ast_node_list_t *list, ast_node_t *node);

ast_node_t *ast_node_make_root(ast_node_list_t tlcs, ast_attribute_list_t attributes, source_location_t source_location);

ast_node_t *ast_node_make_error(diag_t *associated_diagnostic);

ast_node_t *ast_node_make_tlc_module(const char *name, ast_node_list_t tlcs, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_tlc_function(
    const char *name,
    ast_type_function_t *function_type,
    const char **argument_names,
    ast_node_t *statement,
    size_t generic_parameter_count,
    const char **generic_parameters,
    ast_attribute_list_t attributes,
    source_location_t source_location
);
ast_node_t *ast_node_make_tlc_extern(const char *name, ast_type_function_t *function_type, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_tlc_type_definition(const char *name, ast_type_t *type, size_t generic_parameter_count, const char **generic_parameters, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_tlc_declaration(const char *name, ast_type_t *type, ast_node_t *initial, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_tlc_enumeration(const char *name, size_t member_count, const char **members, ast_attribute_list_t attributes, source_location_t source_location);

ast_node_t *ast_node_make_stmt_block(ast_node_list_t statements, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_stmt_declaration(const char *name, ast_type_t *type, ast_node_t *initial, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_stmt_expression(ast_node_t *expression, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_stmt_return(ast_node_t *value, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_stmt_if(ast_node_t *condition, ast_node_t *body, ast_node_t *else_body, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_stmt_while(ast_node_t *condition, ast_node_t *body, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_stmt_continue(ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_stmt_break(ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_stmt_for(ast_node_t *declaration, ast_node_t *condition, ast_node_t *expr_after, ast_node_t *body, ast_attribute_list_t attributes, source_location_t source_location);
ast_node_t *ast_node_make_stmt_switch(ast_node_t *value, size_t case_count, ast_node_switch_case_t *cases, ast_node_t *default_body, ast_attribute_list_t attributes, source_location_t source_location);

ast_node_t *ast_node_make_expr_literal_numeric(uintmax_t value, source_location_t source_location);
ast_node_t *ast_node_make_expr_literal_string(const char *value, source_location_t source_location);
ast_node_t *ast_node_make_expr_literal_char(char value, source_location_t source_location);
ast_node_t *ast_node_make_expr_literal_bool(bool value, source_location_t source_location);
ast_node_t *ast_node_make_expr_literal_struct(const char *type_name, size_t member_count, ast_node_struct_literal_member_t *members, source_location_t source_location);
ast_node_t *ast_node_make_expr_binary(ast_node_binary_operation_t operation, ast_node_t *left, ast_node_t *right, source_location_t source_location);
ast_node_t *ast_node_make_expr_unary(ast_node_unary_operation_t operation, ast_node_t *operand, source_location_t source_location);
ast_node_t *ast_node_make_expr_variable(const char *name, size_t generic_parameter_count, ast_type_t **generic_parameters, source_location_t source_location);
ast_node_t *ast_node_make_expr_call(ast_node_t *function_reference, ast_node_list_t arguments, source_location_t source_location);
ast_node_t *ast_node_make_expr_tuple(ast_node_list_t values, source_location_t source_location);
ast_node_t *ast_node_make_expr_cast(ast_node_t *value, ast_type_t *type, source_location_t source_location);
ast_node_t *ast_node_make_expr_subscript_index(ast_node_t *value, ast_node_t *index, source_location_t source_location);
ast_node_t *ast_node_make_expr_subscript_index_const(ast_node_t *value, uintmax_t index, source_location_t source_location);
ast_node_t *ast_node_make_expr_subscript_member(ast_node_t *value, const char *name, source_location_t source_location);
ast_node_t *ast_node_make_expr_selector(const char *name, ast_node_t *value, source_location_t source_location);
ast_node_t *ast_node_make_expr_sizeof(ast_type_t *type, source_location_t source_location);
