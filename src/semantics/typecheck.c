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
    bool varargs;
} function_t;

typedef enum {
    SCOPE_TYPE_BLOCK,
    SCOPE_TYPE_GLOBAL,
    SCOPE_TYPE_FUNCTION
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
        case SCOPE_TYPE_FUNCTION: break;
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
        case SCOPE_TYPE_FUNCTION: break;
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

static void scope_add_function(scope_t *scope, const char *name, ir_type_t *return_type, size_t argument_count, function_argument_t *arguments, bool varargs) {
    assert(scope != NULL);
    while(scope->type != SCOPE_TYPE_GLOBAL) {
        scope = scope->outer;
        assert(scope != NULL);
    }
    scope->global.functions = realloc(scope->global.functions, sizeof(function_t) * ++scope->global.function_count);
    scope->global.functions[scope->global.function_count - 1] = (function_t) {
        .name = name,
        .return_type = return_type,
        .argument_count = argument_count,
        .arguments = arguments,
        .varargs = varargs
    };
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

static ir_type_t *check_program(scope_t *scope, ir_node_t *node) {
    for(size_t i = 0; i < node->program.function_count; i++) check_common(scope, node->program.functions[i]);
    return NULL;
}

static ir_type_t *check_function(scope_t *scope, ir_node_t *node) {
    size_t argument_count = 0;
    function_argument_t *arguments = NULL;
    for(size_t i = 0; i < node->function.argument_count; i++) {
        arguments = realloc(arguments, sizeof(function_argument_t) * ++argument_count);
        arguments[argument_count - 1] = (function_argument_t) {
            .type = node->function.arguments[i].type,
            .name = node->function.arguments[i].name
        };
    }
    scope_add_function(scope, node->function.name, node->function.type, argument_count, arguments, node->function.varargs);

    scope = make_scope(scope, SCOPE_TYPE_FUNCTION);
    for(size_t i = 0; i < argument_count; i++) scope_add_local(scope, arguments[i].name, arguments[i].type);
    check_common(scope, node->function.body);
    free_scope(scope);
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
    ir_type_t *type_left = check_common(scope, node->expr_binary.left);
    ir_type_t *type_right = check_common(scope, node->expr_binary.right);
    if(ir_type_is_void(type_left) || ir_type_is_void(type_right)) diag_error(node->diag_loc, "void type in binary expression");
    if(ir_type_is_kind(type_left, TYPE_KIND_POINTER) || ir_type_is_kind(type_right, TYPE_KIND_POINTER)) diag_error(node->diag_loc, "pointer type in binary expression");

    ir_type_t *type = type_left;
    switch(node->expr_binary.operation) {
        case IR_BINARY_OPERATION_ASSIGN:
            if(node->expr_binary.left->type != IR_NODE_TYPE_EXPR_BINARY) diag_error(node->diag_loc, "invalid left operand of assignment");
            // TODO: unsafe cast
            if(!ir_type_cmp(type_left, type_right) != 0) node->expr_binary.left = ir_node_make_expr_cast(node->expr_binary.right, type);
            break;
        default:
            // TODO: unsafe cast
            if(ir_type_cmp(type_left, type_right) > 0) node->expr_binary.right = ir_node_make_expr_cast(node->expr_binary.right, type);
            if(ir_type_cmp(type_left, type_right) < 0) {
                type = type_right;
                // TODO: unsafe cast
                node->expr_binary.left = ir_node_make_expr_cast(node->expr_binary.left, type);
            }
            break;
    }
    return type;
}

static ir_type_t *check_expr_unary(scope_t *scope, ir_node_t *node) {
    ir_type_t *type_operand = check_common(scope, node->expr_unary.operand);
    if(ir_type_is_void(type_operand)) diag_error(node->diag_loc, "void type in unary expression");
    if(ir_type_is_kind(type_operand, TYPE_KIND_POINTER)) diag_error(node->diag_loc, "pointer type in unary expression");
    return type_operand;
}

static ir_type_t *check_expr_var(scope_t *scope, ir_node_t *node) {
    variable_t *variable = scope_get_variable(scope, node->expr_var.name);
    if(variable == NULL) diag_error(node->diag_loc, "reference to an undefined variable `%s`", node->expr_var.name);
    return variable->type;
}

static ir_type_t *check_expr_call(scope_t *scope, ir_node_t *node) {
    function_t *function = scope_get_function(scope, node->expr_call.name);
    if(function == NULL) diag_error(node->diag_loc, "reference to an undefined function `%s`", node->expr_call.name);
    if(!function->varargs && function->argument_count != node->expr_call.argument_count) diag_error(node->diag_loc, "invalid number of arguments");
    for(size_t i = 0; i < node->expr_call.argument_count; i++) {
        ir_type_t *type_argument = check_common(scope, node->expr_call.arguments[i]);
        if(ir_type_is_void(type_argument)) diag_error(node->expr_call.arguments[i]->diag_loc, "cannot pass void as an argument");
        if(function->argument_count <= i) continue;
        if(ir_type_cmp(function->arguments[i].type, type_argument) == 0) continue;
        // TODO unsafe cast
        node->expr_call.arguments[i] = ir_node_make_expr_cast(node->expr_call.arguments[i], type_argument);
    }
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
    if(ir_type_is_void(node->stmt_decl.type)) diag_error(node->diag_loc, "cannot declare a variable as void");
    if(node->stmt_decl.initial != NULL) {
        ir_type_t *initial_type = check_common(scope, node->stmt_decl.initial);
        if(ir_type_is_void(initial_type)) diag_error(node->diag_loc, "cannot initialize variable with void");
        // TODO: unsafe cast
        if(ir_type_cmp(node->stmt_decl.type, initial_type) == 0) node->stmt_decl.initial = ir_node_make_expr_cast(node->stmt_decl.initial, node->stmt_decl.type);
    }
    scope_add_local(scope, node->stmt_decl.name, node->stmt_decl.type);
    return NULL;
}

static ir_type_t *check_common(scope_t *scope, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_PROGRAM: return check_program(scope, node);
        case IR_NODE_TYPE_FUNCTION: return check_function(scope, node);

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: return check_expr_literal_numeric(scope, node);
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: return check_expr_literal_string(scope, node);
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: return check_expr_literal_char(scope, node);
        case IR_NODE_TYPE_EXPR_BINARY: return check_expr_binary(scope, node);
        case IR_NODE_TYPE_EXPR_UNARY: return check_expr_unary(scope, node);
        case IR_NODE_TYPE_EXPR_VAR: return check_expr_var(scope, node);
        case IR_NODE_TYPE_EXPR_CALL: return check_expr_call(scope, node);
        case IR_NODE_TYPE_EXPR_CAST: assert(false);

        case IR_NODE_TYPE_STMT_BLOCK: return check_stmt_block(scope, node);
        case IR_NODE_TYPE_STMT_RETURN: return check_stmt_return(scope, node);
        case IR_NODE_TYPE_STMT_IF: return check_stmt_if(scope, node);
        case IR_NODE_TYPE_STMT_DECL: return check_stmt_decl(scope, node);
    }
    assert(false);
}

void typecheck(ir_node_t *ast) {
    scope_t *global_scope = make_scope(NULL, SCOPE_TYPE_GLOBAL);

    // TODO: remove eventually
    function_argument_t *printf_args = malloc(sizeof(function_argument_t));
    printf_args->name = "fmt";
    printf_args->type = ir_type_make_pointer(ir_type_get_u8());
    scope_add_function(global_scope, "printf", ir_type_get_uint(), 1, printf_args, true);

    check_common(global_scope, ast);
    free_scope(global_scope);
}