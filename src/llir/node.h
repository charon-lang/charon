#pragma once

#include "lib/source.h"
#include "llir/type.h"

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

/* variables in body: `node`, `i` */
#define LLIR_NODE_LIST_FOREACH(LIST, BODY) { llir_node_t *__node = (LIST)->first; for(size_t __i = 0; __i < llir_node_list_count(LIST); __i++, __node = __node->next) { assert(__node != NULL); { [[maybe_unused]] llir_node_t *node = __node; [[maybe_unused]] size_t i = __i; BODY; }; }; }
#define LLIR_NODE_LIST_INIT ((llir_node_list_t) { .count = 0, .first = NULL, .last = NULL })

#define LLIR_CASE_TLC(BODY) \
        case LLIR_NODE_TYPE_TLC_MODULE: \
        case LLIR_NODE_TYPE_TLC_FUNCTION: \
        case LLIR_NODE_TYPE_TLC_DECLARATION: \
            BODY; break;

#define LLIR_CASE_STMT(BODY) \
        case LLIR_NODE_TYPE_STMT_BLOCK: \
        case LLIR_NODE_TYPE_STMT_DECLARATION: \
        case LLIR_NODE_TYPE_STMT_EXPRESSION: \
        case LLIR_NODE_TYPE_STMT_RETURN: \
        case LLIR_NODE_TYPE_STMT_IF: \
        case LLIR_NODE_TYPE_STMT_WHILE: \
        case LLIR_NODE_TYPE_STMT_CONTINUE: \
        case LLIR_NODE_TYPE_STMT_BREAK: \
        case LLIR_NODE_TYPE_STMT_FOR: \
            BODY; break;

#define LLIR_CASE_EXPRESSION(BODY) \
        case LLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC: \
        case LLIR_NODE_TYPE_EXPR_LITERAL_STRING: \
        case LLIR_NODE_TYPE_EXPR_LITERAL_CHAR: \
        case LLIR_NODE_TYPE_EXPR_LITERAL_BOOL: \
        case LLIR_NODE_TYPE_EXPR_BINARY: \
        case LLIR_NODE_TYPE_EXPR_UNARY: \
        case LLIR_NODE_TYPE_EXPR_VARIABLE: \
        case LLIR_NODE_TYPE_EXPR_TUPLE: \
        case LLIR_NODE_TYPE_EXPR_CALL: \
        case LLIR_NODE_TYPE_EXPR_CAST: \
        case LLIR_NODE_TYPE_EXPR_SUBSCRIPT: \
        case LLIR_NODE_TYPE_EXPR_SELECTOR: \
            BODY; break;

typedef enum {
    LLIR_NODE_TYPE_ROOT,

    LLIR_NODE_TYPE_TLC_MODULE,
    LLIR_NODE_TYPE_TLC_FUNCTION,
    LLIR_NODE_TYPE_TLC_DECLARATION,

    LLIR_NODE_TYPE_STMT_BLOCK,
    LLIR_NODE_TYPE_STMT_DECLARATION,
    LLIR_NODE_TYPE_STMT_EXPRESSION,
    LLIR_NODE_TYPE_STMT_RETURN,
    LLIR_NODE_TYPE_STMT_IF,
    LLIR_NODE_TYPE_STMT_WHILE,
    LLIR_NODE_TYPE_STMT_CONTINUE,
    LLIR_NODE_TYPE_STMT_BREAK,
    LLIR_NODE_TYPE_STMT_FOR,

    LLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC,
    LLIR_NODE_TYPE_EXPR_LITERAL_STRING,
    LLIR_NODE_TYPE_EXPR_LITERAL_CHAR,
    LLIR_NODE_TYPE_EXPR_LITERAL_BOOL,
    LLIR_NODE_TYPE_EXPR_BINARY,
    LLIR_NODE_TYPE_EXPR_UNARY,
    LLIR_NODE_TYPE_EXPR_VARIABLE,
    LLIR_NODE_TYPE_EXPR_CALL,
    LLIR_NODE_TYPE_EXPR_TUPLE,
    LLIR_NODE_TYPE_EXPR_CAST,
    LLIR_NODE_TYPE_EXPR_SUBSCRIPT,
    LLIR_NODE_TYPE_EXPR_SELECTOR
} llir_node_type_t;

typedef enum {
    LLIR_NODE_BINARY_OPERATION_ASSIGN,
    LLIR_NODE_BINARY_OPERATION_ADDITION,
    LLIR_NODE_BINARY_OPERATION_SUBTRACTION,
    LLIR_NODE_BINARY_OPERATION_MULTIPLICATION,
    LLIR_NODE_BINARY_OPERATION_DIVISION,
    LLIR_NODE_BINARY_OPERATION_MODULO,
    LLIR_NODE_BINARY_OPERATION_GREATER,
    LLIR_NODE_BINARY_OPERATION_GREATER_EQUAL,
    LLIR_NODE_BINARY_OPERATION_LESS,
    LLIR_NODE_BINARY_OPERATION_LESS_EQUAL,
    LLIR_NODE_BINARY_OPERATION_EQUAL,
    LLIR_NODE_BINARY_OPERATION_NOT_EQUAL,
    LLIR_NODE_BINARY_OPERATION_LOGICAL_AND,
    LLIR_NODE_BINARY_OPERATION_LOGICAL_OR,
    LLIR_NODE_BINARY_OPERATION_SHIFT_LEFT,
    LLIR_NODE_BINARY_OPERATION_SHIFT_RIGHT,
    LLIR_NODE_BINARY_OPERATION_AND,
    LLIR_NODE_BINARY_OPERATION_OR,
    LLIR_NODE_BINARY_OPERATION_XOR
} llir_node_binary_operation_t;

typedef enum {
    LLIR_NODE_UNARY_OPERATION_NOT,
    LLIR_NODE_UNARY_OPERATION_NEGATIVE,
    LLIR_NODE_UNARY_OPERATION_DEREF,
    LLIR_NODE_UNARY_OPERATION_REF
} llir_node_unary_operation_t;

typedef enum {
    LLIR_NODE_SUBSCRIPT_TYPE_INDEX,
    LLIR_NODE_SUBSCRIPT_TYPE_INDEX_CONST,
    LLIR_NODE_SUBSCRIPT_TYPE_MEMBER,
} llir_node_subscript_type_t;

typedef struct llir_node llir_node_t;

typedef struct {
    size_t count;
    llir_node_t *first, *last;
} llir_node_list_t;

struct llir_node {
    llir_node_type_t type;
    source_location_t source_location;
    union {
        struct {
            llir_node_list_t tlcs;
        } root;

        struct {
            const char *name;
            llir_node_list_t tlcs;
        } tlc_module;
        struct {
            const char *name;
            const char **argument_names;
            llir_type_function_t *function_type;
            llir_node_t *statement; /* nullable */
        } tlc_function;
        struct {
            const char *name;
            llir_node_t *initial; /* nullable */
        } tlc_declaration;

        struct {
            llir_node_list_t statements;
        } stmt_block;
        struct {
            const char *name;
            llir_type_t *type; /* nullable */
            llir_node_t *initial; /* nullable */
        } stmt_declaration;
        struct {
            llir_node_t *expression;
        } stmt_expression;
        struct {
            llir_node_t *value; /* nullable */
        } stmt_return;
        struct {
            llir_node_t *condition;
            llir_node_t *body, *else_body; /* nullable */
        } stmt_if;
        struct {
            llir_node_t *condition; /* nullable */
            llir_node_t *body; /* nullable */
        } stmt_while;
        struct {
            llir_node_t *declaration, *condition, *expr_after; /* nullable */
            llir_node_t *body; /* nullable */
        } stmt_for;

        struct {
            bool is_const;
            union {
                union {
                    uintmax_t numeric_value;
                    const char *string_value;
                    char char_value;
                    bool bool_value;
                } literal;
                struct {
                    llir_node_binary_operation_t operation;
                    llir_node_t *left, *right;
                } binary;
                struct {
                    llir_node_unary_operation_t operation;
                    llir_node_t *operand;
                } unary;
                struct {
                    const char *name;
                } variable;
                struct {
                    llir_node_t *function_reference;
                    llir_node_list_t arguments;
                } call;
                struct {
                    llir_node_list_t values;
                } tuple;
                struct {
                    llir_node_t *value;
                    llir_type_t *type;
                } cast;
                struct {
                    llir_node_subscript_type_t type;
                    llir_node_t *value;
                    union {
                        uintmax_t index_const;
                        llir_node_t *index;
                        const char *member;
                    };
                } subscript;
                struct {
                    const char *name;
                    llir_node_t *value;
                } selector;
            };
        } expr;
    };
    llir_node_t *next;
};

size_t llir_node_list_count(llir_node_list_t *list);
void llir_node_list_append(llir_node_list_t *list, llir_node_t *node);