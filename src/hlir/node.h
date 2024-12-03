#pragma once

#include "lib/source.h"
#include "hlir/type.h"
#include "hlir/attribute.h"

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

/* variables in body: `node`, `i` */
#define HLIR_NODE_LIST_FOREACH(LIST, BODY) { hlir_node_t *__node = (LIST)->first; for(size_t __i = 0; __i < hlir_node_list_count(LIST); __i++, __node = __node->next) { assert(__node != NULL); { [[maybe_unused]] hlir_node_t *node = __node; [[maybe_unused]] size_t i = __i; BODY; }; }; }
#define HLIR_NODE_LIST_INIT ((hlir_node_list_t) { .count = 0, .first = NULL, .last = NULL })

#define HLIR_CASE_TLC(BODY) \
        case HLIR_NODE_TYPE_TLC_MODULE: \
        case HLIR_NODE_TYPE_TLC_FUNCTION: \
        case HLIR_NODE_TYPE_TLC_EXTERN: \
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION: \
        case HLIR_NODE_TYPE_TLC_DECLARATION: \
        case HLIR_NODE_TYPE_TLC_ENUMERATION: \
            BODY; break;

#define HLIR_CASE_STMT(BODY) \
        case HLIR_NODE_TYPE_STMT_BLOCK: \
        case HLIR_NODE_TYPE_STMT_DECLARATION: \
        case HLIR_NODE_TYPE_STMT_EXPRESSION: \
        case HLIR_NODE_TYPE_STMT_RETURN: \
        case HLIR_NODE_TYPE_STMT_IF: \
        case HLIR_NODE_TYPE_STMT_WHILE: \
        case HLIR_NODE_TYPE_STMT_CONTINUE: \
        case HLIR_NODE_TYPE_STMT_BREAK: \
        case HLIR_NODE_TYPE_STMT_FOR: \
            BODY; break;

#define HLIR_CASE_EXPRESSION(BODY) \
        case HLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC: \
        case HLIR_NODE_TYPE_EXPR_LITERAL_STRING: \
        case HLIR_NODE_TYPE_EXPR_LITERAL_CHAR: \
        case HLIR_NODE_TYPE_EXPR_LITERAL_BOOL: \
        case HLIR_NODE_TYPE_EXPR_BINARY: \
        case HLIR_NODE_TYPE_EXPR_UNARY: \
        case HLIR_NODE_TYPE_EXPR_VARIABLE: \
        case HLIR_NODE_TYPE_EXPR_TUPLE: \
        case HLIR_NODE_TYPE_EXPR_CALL: \
        case HLIR_NODE_TYPE_EXPR_CAST: \
        case HLIR_NODE_TYPE_EXPR_SUBSCRIPT: \
        case HLIR_NODE_TYPE_EXPR_SELECTOR: \
        case HLIR_NODE_TYPE_EXPR_SIZEOF: \
            BODY; break;

typedef enum {
    HLIR_NODE_TYPE_ROOT,

    HLIR_NODE_TYPE_TLC_MODULE,
    HLIR_NODE_TYPE_TLC_FUNCTION,
    HLIR_NODE_TYPE_TLC_EXTERN,
    HLIR_NODE_TYPE_TLC_TYPE_DEFINITION,
    HLIR_NODE_TYPE_TLC_DECLARATION,
    HLIR_NODE_TYPE_TLC_ENUMERATION,

    HLIR_NODE_TYPE_STMT_BLOCK,
    HLIR_NODE_TYPE_STMT_DECLARATION,
    HLIR_NODE_TYPE_STMT_EXPRESSION,
    HLIR_NODE_TYPE_STMT_RETURN,
    HLIR_NODE_TYPE_STMT_IF,
    HLIR_NODE_TYPE_STMT_WHILE,
    HLIR_NODE_TYPE_STMT_CONTINUE,
    HLIR_NODE_TYPE_STMT_BREAK,
    HLIR_NODE_TYPE_STMT_FOR,

    HLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC,
    HLIR_NODE_TYPE_EXPR_LITERAL_STRING,
    HLIR_NODE_TYPE_EXPR_LITERAL_CHAR,
    HLIR_NODE_TYPE_EXPR_LITERAL_BOOL,
    HLIR_NODE_TYPE_EXPR_BINARY,
    HLIR_NODE_TYPE_EXPR_UNARY,
    HLIR_NODE_TYPE_EXPR_VARIABLE,
    HLIR_NODE_TYPE_EXPR_CALL,
    HLIR_NODE_TYPE_EXPR_TUPLE,
    HLIR_NODE_TYPE_EXPR_CAST,
    HLIR_NODE_TYPE_EXPR_SUBSCRIPT,
    HLIR_NODE_TYPE_EXPR_SELECTOR,
    HLIR_NODE_TYPE_EXPR_SIZEOF
} hlir_node_type_t;

typedef enum {
    HLIR_NODE_BINARY_OPERATION_ASSIGN,
    HLIR_NODE_BINARY_OPERATION_ADDITION,
    HLIR_NODE_BINARY_OPERATION_SUBTRACTION,
    HLIR_NODE_BINARY_OPERATION_MULTIPLICATION,
    HLIR_NODE_BINARY_OPERATION_DIVISION,
    HLIR_NODE_BINARY_OPERATION_MODULO,
    HLIR_NODE_BINARY_OPERATION_GREATER,
    HLIR_NODE_BINARY_OPERATION_GREATER_EQUAL,
    HLIR_NODE_BINARY_OPERATION_LESS,
    HLIR_NODE_BINARY_OPERATION_LESS_EQUAL,
    HLIR_NODE_BINARY_OPERATION_EQUAL,
    HLIR_NODE_BINARY_OPERATION_NOT_EQUAL,
    HLIR_NODE_BINARY_OPERATION_LOGICAL_AND,
    HLIR_NODE_BINARY_OPERATION_LOGICAL_OR,
    HLIR_NODE_BINARY_OPERATION_SHIFT_LEFT,
    HLIR_NODE_BINARY_OPERATION_SHIFT_RIGHT,
    HLIR_NODE_BINARY_OPERATION_AND,
    HLIR_NODE_BINARY_OPERATION_OR,
    HLIR_NODE_BINARY_OPERATION_XOR
} hlir_node_binary_operation_t;

typedef enum {
    HLIR_NODE_UNARY_OPERATION_NOT,
    HLIR_NODE_UNARY_OPERATION_NEGATIVE,
    HLIR_NODE_UNARY_OPERATION_DEREF,
    HLIR_NODE_UNARY_OPERATION_REF
} hlir_node_unary_operation_t;

typedef enum {
    HLIR_NODE_SUBSCRIPT_TYPE_INDEX,
    HLIR_NODE_SUBSCRIPT_TYPE_INDEX_CONST,
    HLIR_NODE_SUBSCRIPT_TYPE_MEMBER,
} hlir_node_subscript_type_t;

typedef struct hlir_node hlir_node_t;

typedef struct {
    size_t count;
    hlir_node_t *first, *last;
} hlir_node_list_t;

struct hlir_node {
    hlir_node_type_t type;
    source_location_t source_location;
    hlir_attribute_list_t attributes;
    union {
        struct {
            hlir_node_list_t tlcs;
        } root;

        struct {
            const char *name;
            hlir_node_list_t tlcs;
        } tlc_module;
        struct {
            const char *name;
            const char **argument_names;
            hlir_type_function_t *function_type;
            hlir_node_t *statement; /* nullable */
        } tlc_function;
        struct {
            const char *name;
            hlir_type_function_t *function_type;
        } tlc_extern;
        struct {
            const char *name;
            hlir_type_t *type;
        } tlc_type_definition;
        struct {
            const char *name;
            hlir_type_t *type;
            hlir_node_t *initial; /* nullable */
        } tlc_declaration;
        struct {
            const char *name;
            size_t member_count;
            const char **members;
        } tlc_enumeration;

        struct {
            hlir_node_list_t statements;
        } stmt_block;
        struct {
            const char *name;
            hlir_type_t *type; /* nullable */
            hlir_node_t *initial; /* nullable */
        } stmt_declaration;
        struct {
            hlir_node_t *expression;
        } stmt_expression;
        struct {
            hlir_node_t *value; /* nullable */
        } stmt_return;
        struct {
            hlir_node_t *condition;
            hlir_node_t *body, *else_body; /* nullable */
        } stmt_if;
        struct {
            hlir_node_t *condition; /* nullable */
            hlir_node_t *body; /* nullable */
        } stmt_while;
        struct {
            hlir_node_t *declaration, *condition, *expr_after; /* nullable */
            hlir_node_t *body; /* nullable */
        } stmt_for;

        union {
            uintmax_t numeric_value;
            const char *string_value;
            char char_value;
            bool bool_value;
        } expr_literal;
        struct {
            hlir_node_binary_operation_t operation;
            hlir_node_t *left, *right;
        } expr_binary;
        struct {
            hlir_node_unary_operation_t operation;
            hlir_node_t *operand;
        } expr_unary;
        struct {
            const char *name;
        } expr_variable;
        struct {
            hlir_node_t *function_reference;
            hlir_node_list_t arguments;
        } expr_call;
        struct {
            hlir_node_list_t values;
        } expr_tuple;
        struct {
            hlir_node_t *value;
            hlir_type_t *type;
        } expr_cast;
        struct {
            hlir_node_subscript_type_t type;
            hlir_node_t *value;
            union {
                uintmax_t index_const;
                hlir_node_t *index;
                const char *member;
            };
        } expr_subscript;
        struct {
            const char *name;
            hlir_node_t *value;
        } expr_selector;
        struct {
            hlir_type_t *type;
        } expr_sizeof;
    };
    hlir_node_t *next;
};

size_t hlir_node_list_count(hlir_node_list_t *list);
void hlir_node_list_append(hlir_node_list_t *list, hlir_node_t *node);

hlir_node_t *hlir_node_make_root(hlir_node_list_t tlcs, hlir_attribute_list_t attributes, source_location_t source_location);

hlir_node_t *hlir_node_make_tlc_module(const char *name, hlir_node_list_t tlcs, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_tlc_function(const char *name, hlir_type_function_t *function_type, const char **argument_names, hlir_node_t *statement, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_tlc_extern(const char *name, hlir_type_function_t *function_type, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_tlc_type_definition(const char *name, hlir_type_t *type, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_tlc_declaration(const char *name, hlir_type_t *type, hlir_node_t *initial, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_tlc_enumeration(const char *name, size_t member_count, const char **members, hlir_attribute_list_t attributes, source_location_t source_location);

hlir_node_t *hlir_node_make_stmt_block(hlir_node_list_t statements, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_stmt_declaration(const char *name, hlir_type_t *type, hlir_node_t *initial, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_stmt_expression(hlir_node_t *expression, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_stmt_return(hlir_node_t *value, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_stmt_if(hlir_node_t *condition, hlir_node_t *body, hlir_node_t *else_body, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_stmt_while(hlir_node_t *condition, hlir_node_t *body, hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_stmt_continue(hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_stmt_break(hlir_attribute_list_t attributes, source_location_t source_location);
hlir_node_t *hlir_node_make_stmt_for(hlir_node_t *declaration, hlir_node_t *condition, hlir_node_t *expr_after, hlir_node_t *body, hlir_attribute_list_t attributes, source_location_t source_location);

hlir_node_t *hlir_node_make_expr_literal_numeric(uintmax_t value, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_literal_string(const char *value, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_literal_char(char value, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_literal_bool(bool value, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_binary(hlir_node_binary_operation_t operation, hlir_node_t *left, hlir_node_t *right, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_unary(hlir_node_unary_operation_t operation, hlir_node_t *operand, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_variable(const char *name, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_call(hlir_node_t *function_reference, hlir_node_list_t arguments, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_tuple(hlir_node_list_t values, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_cast(hlir_node_t *value, hlir_type_t *type, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_subscript_index(hlir_node_t *value, hlir_node_t *index, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_subscript_index_const(hlir_node_t *value, uintmax_t index, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_subscript_member(hlir_node_t *value, const char *name, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_selector(const char *name, hlir_node_t *value, source_location_t source_location);
hlir_node_t *hlir_node_make_expr_sizeof(hlir_type_t *type, source_location_t source_location);