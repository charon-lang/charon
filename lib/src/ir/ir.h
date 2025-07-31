#pragma once

#include "lib/source.h"
#include "lib/vector.h"

#include <stddef.h>
#include <stdint.h>

#define IR_NAMESPACE_INIT ((ir_namespace_t) { .symbol_count = 0, .symbols = NULL, .type_count = 0, .types = NULL })

typedef struct ir_namespace ir_namespace_t;
typedef struct ir_scope ir_scope_t;
typedef struct ir_variable ir_variable_t;
typedef struct ir_function_prototype ir_function_prototype_t;
typedef struct ir_function ir_function_t;
typedef struct ir_module ir_module_t;
typedef struct ir_enumeration ir_enumeration_t;
typedef struct ir_symbol ir_symbol_t;
typedef struct ir_type_declaration ir_type_declaration_t;
typedef struct ir_type_cache ir_type_cache_t;
typedef struct ir_type ir_type_t;
typedef struct ir_type_structure_member ir_type_structure_member_t;
typedef struct ir_switch_case ir_switch_case_t;
typedef struct ir_unit ir_unit_t;
typedef struct ir_literal_struct_member ir_literal_struct_member_t;
typedef struct ir_expr ir_expr_t;
typedef struct ir_stmt ir_stmt_t;

VECTOR_DEFINE(expr, ir_expr_t *)
VECTOR_DEFINE(stmt, ir_stmt_t *)

typedef enum {
    IR_TYPE_KIND_VOID,
    IR_TYPE_KIND_INTEGER,
    IR_TYPE_KIND_POINTER,
    IR_TYPE_KIND_TUPLE,
    IR_TYPE_KIND_ARRAY,
    IR_TYPE_KIND_STRUCTURE,
    IR_TYPE_KIND_FUNCTION_REFERENCE,
    IR_TYPE_KIND_ENUMERATION
} ir_type_kind_t;

typedef enum {
    IR_SYMBOL_KIND_MODULE,
    IR_SYMBOL_KIND_FUNCTION,
    IR_SYMBOL_KIND_VARIABLE,
    IR_SYMBOL_KIND_ENUMERATION
} ir_symbol_kind_t;

typedef enum {
    IR_STMT_KIND_BLOCK,
    IR_STMT_KIND_DECLARATION,
    IR_STMT_KIND_EXPRESSION,
    IR_STMT_KIND_RETURN,
    IR_STMT_KIND_IF,
    IR_STMT_KIND_WHILE,
    IR_STMT_KIND_CONTINUE,
    IR_STMT_KIND_BREAK,
    IR_STMT_KIND_FOR,
    IR_STMT_KIND_SWITCH
} ir_stmt_kind_t;

typedef enum {
    IR_EXPR_KIND_LITERAL_NUMERIC,
    IR_EXPR_KIND_LITERAL_STRING,
    IR_EXPR_KIND_LITERAL_CHAR,
    IR_EXPR_KIND_LITERAL_BOOL,
    IR_EXPR_KIND_LITERAL_STRUCT,
    IR_EXPR_KIND_BINARY,
    IR_EXPR_KIND_UNARY,
    IR_EXPR_KIND_VARIABLE,
    IR_EXPR_KIND_CALL,
    IR_EXPR_KIND_TUPLE,
    IR_EXPR_KIND_CAST,
    IR_EXPR_KIND_SUBSCRIPT,
    IR_EXPR_KIND_SELECTOR,
    IR_EXPR_KIND_SIZEOF,
    IR_EXPR_KIND_ENUMERATION_VALUE
} ir_expr_kind_t;

typedef enum {
    IR_BINARY_OPERATION_ASSIGN,
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
    IR_BINARY_OPERATION_LOGICAL_AND,
    IR_BINARY_OPERATION_LOGICAL_OR,
    IR_BINARY_OPERATION_SHIFT_LEFT,
    IR_BINARY_OPERATION_SHIFT_RIGHT,
    IR_BINARY_OPERATION_AND,
    IR_BINARY_OPERATION_OR,
    IR_BINARY_OPERATION_XOR
} ir_binary_operation_t;

typedef enum {
    IR_UNARY_OPERATION_NOT,
    IR_UNARY_OPERATION_NEGATIVE,
    IR_UNARY_OPERATION_DEREF,
    IR_UNARY_OPERATION_REF
} ir_unary_operation_t;

typedef enum {
    IR_SUBSCRIPT_KIND_INDEX,
    IR_SUBSCRIPT_KIND_INDEX_CONST,
    IR_SUBSCRIPT_KIND_MEMBER,
} ir_subscript_kind_t;

struct ir_function_prototype {
    ir_type_t *return_type;
    size_t argument_count;
    ir_type_t **arguments;
    bool varargs;
};

struct ir_variable {
    const char *name;
    ir_type_t *type;
    ir_expr_t *initial_value; /* nullable */
    bool is_local;
    union {
        ir_scope_t *scope;
        ir_module_t *module;
    };
    void *codegen_data;
};

struct ir_function {
    const char *name;
    const char *link_name;
    bool is_extern;
    ir_function_prototype_t prototype;
    ir_stmt_t *statement; /* nullable */
    ir_variable_t **arguments; // TODO: possible have some sort of stmt to contain the scope&args
    ir_scope_t *scope;
    void *codegen_data;
    source_location_t source_location;
};

struct ir_enumeration {
    const char *name;
    ir_type_t *type;
    const char **members;
};

struct ir_scope {
    ir_scope_t *parent;
    ir_variable_t **variables;
    size_t variable_count;
};

struct ir_symbol {
    ir_symbol_kind_t kind;
    union {
        ir_module_t *module;
        ir_function_t *function;
        ir_variable_t *variable;
        ir_enumeration_t *enumeration;
    };
};

struct ir_type_declaration {
    const char *name;
    ir_type_t *type;
};

struct ir_namespace {
    size_t symbol_count;
    ir_symbol_t **symbols;

    size_t type_count;
    ir_type_declaration_t *types;
};

struct ir_module {
    const char *name;
    ir_module_t *parent;
    ir_namespace_t namespace;
};

struct ir_type {
    ir_type_kind_t kind;
    union {
        struct {
            bool allow_coerce_pointer;
            bool is_signed;
            size_t bit_size;
        } integer;
        struct {
            ir_type_t *pointee;
        } pointer;
        struct {
            size_t type_count;
            ir_type_t **types;
        } tuple;
        struct {
            ir_type_t *type;
            size_t size;
        } array;
        struct {
            bool packed;
            size_t member_count;
            ir_type_structure_member_t *members;
        } structure;
        struct {
            ir_function_prototype_t prototype;
        } function_reference;
        struct {
            size_t bit_size;
            size_t member_count;
        } enumeration;
    };
};

struct ir_type_structure_member {
    ir_type_t *type;
    const char *name;
};

struct ir_switch_case {
    ir_expr_t *value;
    ir_scope_t *body_scope;
    ir_stmt_t *body;
};

struct ir_type_cache {
    ir_type_t **types;
    size_t type_count;

    ir_type_t **defined_types;
    size_t defined_type_count;
};

struct ir_stmt {
    source_location_t source_location;
    ir_stmt_kind_t kind;
    union {
        struct {
            ir_scope_t *scope;
            vector_stmt_t statements;
        } block;
        struct {
            ir_variable_t *variable;
        } declaration;
        struct {
            ir_expr_t *expression;
        } expression;
        struct {
            ir_expr_t *value; /* nullable */
        } _return;
        struct {
            ir_expr_t *condition;
            ir_stmt_t *body; /* nullable */
            ir_stmt_t *else_body; /* nullable */
        } _if;
        struct {
            ir_expr_t *condition; /* nullable */
            ir_stmt_t *body; /* nullable */
        } _while;
        struct {
            ir_scope_t *scope;
            ir_stmt_t *declaration; /* nullable */
            ir_expr_t *condition; /* nullable */
            ir_expr_t *expr_after; /* nullable */
            ir_stmt_t *body; /* nullable */
        } _for;
        struct {
            ir_expr_t *value;
            size_t case_count;
            ir_switch_case_t *cases;
            ir_stmt_t *default_body; /* nullable */
            ir_scope_t *default_body_scope; /* nullable */
        } _switch;
    };
};

struct ir_literal_struct_member {
    ir_expr_t *value;
    source_location_t source_location;
};

struct ir_expr {
    source_location_t source_location;
    ir_expr_kind_t kind;
    bool is_const;
    ir_type_t *type;
    union {
        union {
            uintmax_t numeric_value;
            const char *string_value;
            char char_value;
            bool bool_value;
            struct {
                ir_type_t *type;
                ir_literal_struct_member_t **members;
            } struct_value;
        } literal;
        struct {
            ir_binary_operation_t operation;
            ir_expr_t *left, *right;
        } binary;
        struct {
            ir_unary_operation_t operation;
            ir_expr_t *operand;
        } unary;
        struct {
            bool is_function;
            union {
                ir_function_t *function;
                ir_variable_t *variable;
            };
        } variable;
        struct {
            ir_expr_t *function_reference;
            vector_expr_t arguments;
        } call;
        struct {
            vector_expr_t values;
        } tuple;
        struct {
            ir_expr_t *value;
            ir_type_t *type;
        } cast;
        struct {
            ir_expr_t *value;
            ir_subscript_kind_t kind;
            union {
                uintmax_t index_const;
                ir_expr_t *index;
                const char *member;
            };
        } subscript;
        struct {
            ir_module_t *module;
            ir_expr_t *value;
        } selector;
        struct {
            ir_type_t *type;
        } _sizeof;
        struct {
            uintmax_t value;
            ir_enumeration_t *enumeration;
        } enumeration_value;
    };
};

struct ir_unit {
    ir_namespace_t root_namespace;
};

ir_variable_t *ir_scope_add(ir_scope_t *scope, const char *name, ir_type_t *type, ir_expr_t *initial_value);
ir_variable_t *ir_scope_find_variable(ir_scope_t *scope, const char *name);


ir_symbol_t *ir_namespace_find_symbol(ir_namespace_t *namespace, const char *name);
ir_symbol_t *ir_namespace_find_symbol_of_kind(ir_namespace_t *namespace, const char *name, ir_symbol_kind_t kind);
bool ir_namespace_exists_symbol(ir_namespace_t *namespace, const char *name);
ir_symbol_t *ir_namespace_add_symbol(ir_namespace_t *namespace, ir_symbol_kind_t kind);

ir_type_declaration_t *ir_namespace_find_type(ir_namespace_t *namespace, const char *name);
bool ir_namespace_exists_type(ir_namespace_t *namespace, const char *name);
void ir_namespace_add_type(ir_namespace_t *namespace, const char *name, ir_type_t *type);


bool ir_type_eq(ir_type_t *a, ir_type_t *b);

ir_type_cache_t *ir_type_cache_make();

ir_type_t *ir_type_cache_get_void(ir_type_cache_t *cache);
ir_type_t *ir_type_cache_get_integer(ir_type_cache_t *cache, size_t bit_size, bool is_signed, bool allow_coerce_pointer);
ir_type_t *ir_type_cache_get_pointer(ir_type_cache_t *cache, ir_type_t *pointee);
ir_type_t *ir_type_cache_get_tuple(ir_type_cache_t *cache, size_t type_count, ir_type_t **types);
ir_type_t *ir_type_cache_get_array(ir_type_cache_t *cache, ir_type_t *element_type, size_t size);
ir_type_t *ir_type_cache_get_structure(ir_type_cache_t *cache, size_t member_count, ir_type_structure_member_t *members, bool packed);
ir_type_t *ir_type_cache_get_function_reference(ir_type_cache_t *cache, ir_function_prototype_t prototype);
ir_type_t *ir_type_cache_get_enumeration(ir_type_cache_t *cache, size_t bit_size, size_t member_count);
ir_type_t *ir_type_cache_define(ir_type_cache_t *cache);
