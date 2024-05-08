#include "typecheck.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "../ir/type.h"
#include "../diag.h"

typedef struct {
    const char *name;
    ir_type_t *type;
} variable_t;

typedef struct {
    const char *name;
    ir_type_t *type;
} function_argument_t;

typedef struct {
    const char *name;
    ir_type_t *return_type;
    size_t argument_count;
    function_argument_t *arguments;
} function_t;

typedef enum {
    SCOPE_TYPE_BLOCK,
    SCOPE_TYPE_GLOBAL
} scope_type_t;

typedef struct scope {
    struct scope *outer;
    scope_type_t type;
    size_t variable_count;
    variable_t *variables;
    union {
        struct {
            size_t function_count;
            function_t *functions;
        } global;
    };
} scope_t;

static scope_t *make_scope(scope_t *outer, scope_type_t type) {
    scope_t *scope = malloc(sizeof(scope_t));
    scope->outer = outer;
    scope->type = type;
    scope->variable_count = 0;
    scope->variables = NULL;
    switch(scope->type) {
        case SCOPE_TYPE_BLOCK: break;
        case SCOPE_TYPE_GLOBAL:
            scope->global.function_count = 0;
            scope->global.functions = NULL;
            break;
    }
    return scope;
}

static void free_scope(scope_t *scope) {
    if(scope->variables != NULL) free(scope->variables);
    switch(scope->type) {
        case SCOPE_TYPE_BLOCK: break;
        case SCOPE_TYPE_GLOBAL:
            if(scope->global.functions == NULL) break;
            for(size_t i = 0; i < scope->global.function_count; i++) {
                if(scope->global.functions[i].arguments == NULL) continue;
                free(scope->global.functions[i].arguments);
            }
            free(scope->global.functions);
            break;
    }
    free(scope);
}

static void scope_add_local(scope_t *scope, const char *name, ir_type_t *type) {
    assert(scope != NULL);
    scope->variables = realloc(scope->variables, sizeof(variable_t) * ++scope->variable_count);
    scope->variables[scope->variable_count - 1] = (variable_t) { .name = name, .type = type };
}

static variable_t *scope_get_variable(scope_t *scope, const char *name) {
    if(scope == NULL) return NULL;
    for(size_t i = 0; i < scope->variable_count; i++) {
        if(strcmp(name, scope->variables[i].name) != 0) continue;
        return &scope->variables[i];
    }
    return scope_get_variable(scope->outer, name);
}

static void scope_add_function(scope_t *scope, const char *name, ir_type_t *return_type, size_t argument_count, function_argument_t *arguments) {
    assert(scope != NULL);
    while(scope->type != SCOPE_TYPE_GLOBAL) {
        scope = scope->outer;
        assert(scope != NULL);
    }
    scope->global.functions = realloc(scope->global.functions, sizeof(function_t) * ++scope->global.function_count);
    scope->global.functions[scope->global.function_count - 1] = (function_t) { .name = name, .return_type = return_type, .argument_count = argument_count, .arguments = arguments };
}

static function_t *scope_get_function(scope_t *scope, const char *name) {
    if(scope == NULL) return NULL;
    if(scope->type == SCOPE_TYPE_GLOBAL) {
        for(size_t i = 0; i < scope->global.function_count; i++) {
            if(strcmp(name, scope->global.functions[i].name) != 0) continue;
            return &scope->global.functions[i];
        }
    }
    return scope_get_function(scope->outer, name);
}

static ir_type_t *check_common(scope_t *scope, ir_node_t *node);

static ir_type_t *check_function(scope_t *scope, ir_node_t *node) {
    scope_add_function(scope, node->function.name, node->function.type, 0, NULL); // TODO: arguments
    check_common(scope, node->function.body);
    return NULL;
}

static ir_type_t *check_expr_literal_numeric(scope_t *scope, ir_node_t *node) {
    return ir_type_get_uint();
}

static ir_type_t *check_expr_literal_string(scope_t *scope, ir_node_t *node) {
    return ir_type_make_pointer(ir_type_get_u8());
}

static ir_type_t *check_expr_literal_char(scope_t *scope, ir_node_t *node) {
    return ir_type_get_u8();
}

static ir_type_t *check_expr_binary(scope_t *scope, ir_node_t *node) {
    // TODO: add check: type works with operation
    ir_type_t *type_left = check_common(scope, node->expr_binary.left);
    [[maybe_unused]] ir_type_t *type_right = check_common(scope, node->expr_binary.right); // TODO: maybe unused
    return type_left; // TODO: cast to a unified type
}

static ir_type_t *check_expr_unary(scope_t *scope, ir_node_t *node) {
    // TODO: add check: type works with operation
    return check_common(scope, node->expr_unary.operand);
}

static ir_type_t *check_expr_var(scope_t *scope, ir_node_t *node) {
    variable_t *variable = scope_get_variable(scope, node->expr_var.name);
    if(variable == NULL) diag_error(node->diag_loc, "reference to an undefined variable `%s`", node->expr_var.name);
    return variable->type;
}

static ir_type_t *check_expr_call(scope_t *scope, ir_node_t *node) {
    function_t *function = scope_get_function(scope, node->expr_call.name);
    if(function == NULL) diag_error(node->diag_loc, "reference to an undefined function `%s`", node->expr_call.name);
    // TODO check arguments
    return function->return_type;
}

static ir_type_t *check_stmt_block(scope_t *scope, ir_node_t *node) {
    scope = make_scope(scope, SCOPE_TYPE_BLOCK);
    for(size_t i = 0; i < node->stmt_block.statement_count; i++) check_common(scope, node->stmt_block.statements[i]);
    free_scope(scope);
    return NULL;
}

static ir_type_t *check_stmt_return(scope_t *scope, ir_node_t *node) {
    if(node->stmt_return.value != NULL) check_common(scope, node->stmt_return.value);
    return NULL;
}

static ir_type_t *check_stmt_if(scope_t *scope, ir_node_t *node) {
    check_common(scope, node->stmt_if.condition);
    check_common(scope, node->stmt_if.body);
    if(node->stmt_if.else_body != NULL) check_common(scope, node->stmt_if.else_body);
    return NULL;
}

static ir_type_t *check_stmt_decl(scope_t *scope, ir_node_t *node) {
    if(scope_get_variable(scope, node->stmt_decl.name)) diag_error(node->diag_loc, "redeclaration of `%s`", node->stmt_decl.name);
    if(node->stmt_decl.initial != NULL) {
        [[maybe_unused]] ir_type_t *initial_type = check_common(scope, node->stmt_decl.initial); // TODO: maybe unused
        // TODO: try to cast initial type
    }
    scope_add_local(scope, node->stmt_decl.name, node->stmt_decl.type);
    return NULL;
}

static ir_type_t *check_common(scope_t *scope, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_FUNCTION: return check_function(scope, node);

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: return check_expr_literal_numeric(scope, node);
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: return check_expr_literal_string(scope, node);
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: return check_expr_literal_char(scope, node);
        case IR_NODE_TYPE_EXPR_BINARY: return check_expr_binary(scope, node);
        case IR_NODE_TYPE_EXPR_UNARY: return check_expr_unary(scope, node);
        case IR_NODE_TYPE_EXPR_VAR: return check_expr_var(scope, node);
        case IR_NODE_TYPE_EXPR_CALL: return check_expr_call(scope, node);

        case IR_NODE_TYPE_STMT_BLOCK: return check_stmt_block(scope, node);
        case IR_NODE_TYPE_STMT_RETURN: return check_stmt_return(scope, node);
        case IR_NODE_TYPE_STMT_IF: return check_stmt_if(scope, node);
        case IR_NODE_TYPE_STMT_DECL: return check_stmt_decl(scope, node);
    }
    assert(false);
}

void typecheck(ir_node_t *ast) {
    scope_t *global_scope = make_scope(NULL, SCOPE_TYPE_GLOBAL);
    scope_add_function(global_scope, "printf", ir_type_get_uint(), 0, NULL); // TODO: remove eventually
    check_common(global_scope, ast);
    free_scope(global_scope);
}