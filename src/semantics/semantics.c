#include "semantics.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "scope.h"
#include "../ir/type.h"
#include "../diag.h"

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

typedef struct {
    scope_t *scope;
    size_t function_count;
    function_t *functions;
} semantics_context_t;

static void ctx_add_function(semantics_context_t *ctx, const char *name, ir_type_t *return_type, size_t argument_count, function_argument_t *arguments, bool varargs) {
    ctx->functions = realloc(ctx->functions, sizeof(function_t) * ++ctx->function_count);
    ctx->functions[ctx->function_count - 1] = (function_t) {
        .name = name,
        .return_type = return_type,
        .argument_count = argument_count,
        .arguments = arguments,
        .varargs = varargs
    };
}

static function_t *ctx_get_function(semantics_context_t *ctx, const char *name) {
    for(size_t i = 0; i < ctx->function_count; i++) {
        if(strcmp(name, ctx->functions[i].name) != 0) continue;
        return &ctx->functions[i];
    }
    return NULL;
}

static ir_node_t *implicit_cast(ir_node_t *value, ir_type_t *from, ir_type_t *to) {
    if(from->kind != to->kind) diag_error(value->diag_loc, "cannot implicitly cast %i to %i", from->kind, to->kind);
    if(ir_type_is_kind(from, IR_TYPE_KIND_POINTER)) {
        while(ir_type_is_kind(from, IR_TYPE_KIND_POINTER) && ir_type_is_kind(to, IR_TYPE_KIND_POINTER)) {
            from = from->pointer.base;
            to = to->pointer.base;
        }
        if(ir_type_cmp(from, to) != 0) diag_warn(value->diag_loc, "implicit cast of different types of pointers");
    }
    if(ir_type_cmp(from, to) == 0) return value;
    return ir_node_make_expr_cast(value, to);
}

static ir_type_t *check_common(semantics_context_t *ctx, ir_node_t *node);

static void check_program(semantics_context_t *ctx, ir_node_t *node) {
    for(size_t i = 0; i < node->program.global_count; i++) check_common(ctx, node->program.globals[i]);
}

static void check_global_function(semantics_context_t *ctx, ir_node_t *node) {
    size_t argument_count = node->global_function.decl.argument_count;
    function_argument_t *arguments = malloc(sizeof(function_argument_t) * argument_count);
    for(size_t i = 0; i < node->global_function.decl.argument_count; i++) {
        arguments[i].name = node->global_function.decl.arguments[i].name;
        arguments[i].type = node->global_function.decl.arguments[i].type;
    }
    ctx_add_function(ctx, node->global_function.decl.name, node->global_function.decl.return_type, argument_count, arguments, node->global_function.decl.varargs);

    ctx->scope = scope_make(ctx->scope, SCOPE_TYPE_FUNCTION);
    for(size_t i = 0; i < argument_count; i++) scope_add_variable(ctx->scope, arguments[i].name, arguments[i].type);
    check_common(ctx, node->global_function.body);
    ctx->scope = scope_free(ctx->scope);
}

static void check_global_extern(semantics_context_t *ctx, ir_node_t *node) {
    size_t argument_count = node->global_extern.decl.argument_count;
    function_argument_t *arguments = malloc(sizeof(function_argument_t) * argument_count);
    for(size_t i = 0; i < node->global_extern.decl.argument_count; i++) {
        arguments[i].name = node->global_extern.decl.arguments[i].name;
        arguments[i].type = node->global_extern.decl.arguments[i].type;
    }
    ctx_add_function(ctx, node->global_extern.decl.name, node->global_extern.decl.return_type, argument_count, arguments, node->global_extern.decl.varargs);
}

static ir_type_t *check_expr_literal_numeric(semantics_context_t *ctx, ir_node_t *node) {
    return ir_type_get_uint();
}

static ir_type_t *check_expr_literal_string(semantics_context_t *ctx, ir_node_t *node) {
    return ir_type_make_pointer(ir_type_get_u8());
}

static ir_type_t *check_expr_literal_char(semantics_context_t *ctx, ir_node_t *node) {
    return ir_type_get_u8();
}

static ir_type_t *check_expr_binary(semantics_context_t *ctx, ir_node_t *node) {
    ir_type_t *type_left = check_common(ctx, node->expr_binary.left);
    ir_type_t *type_right = check_common(ctx, node->expr_binary.right);
    if(ir_type_is_void(type_left) || ir_type_is_void(type_right)) diag_error(node->diag_loc, "void type in binary expression");
    if(ir_type_is_kind(type_left, IR_TYPE_KIND_POINTER) || ir_type_is_kind(type_right, IR_TYPE_KIND_POINTER)) diag_error(node->diag_loc, "pointer type in binary expression");

    ir_type_t *type = type_left;
    switch(node->expr_binary.operation) {
        case IR_BINARY_OPERATION_ASSIGN:
            if(node->expr_binary.left->type != IR_NODE_TYPE_EXPR_VAR && (
                node->expr_binary.left->type != IR_NODE_TYPE_EXPR_UNARY ||
                node->expr_binary.left->expr_unary.operation != IR_UNARY_OPERATION_DEREF
            )) diag_error(node->diag_loc, "invalid left operand of assignment");
            node->expr_binary.right = implicit_cast(node->expr_binary.right, type_right, type);
            break;
        default:
            if(ir_type_cmp(type_left, type_right) > 0) {
                node->expr_binary.right = implicit_cast(node->expr_binary.right, type_right, type);
            }
            if(ir_type_cmp(type_left, type_right) < 0) {
                type = type_right;
                node->expr_binary.left = implicit_cast(node->expr_binary.left, type_left, type);
            }
            break;
    }
    return type;
}

static ir_type_t *check_expr_unary(semantics_context_t *ctx, ir_node_t *node) {
    ir_type_t *type_operand = check_common(ctx, node->expr_unary.operand);
    if(ir_type_is_void(type_operand)) diag_error(node->diag_loc, "void type in unary expression");
    if(node->expr_unary.operation == IR_UNARY_OPERATION_DEREF) {
        if(!ir_type_is_kind(type_operand, IR_TYPE_KIND_POINTER)) diag_error(node->diag_loc, "cannot dereference a non-pointer");
        return type_operand->pointer.base;
    }
    if(node->expr_unary.operation == IR_UNARY_OPERATION_REF) {
        if(node->expr_unary.operand->type != IR_NODE_TYPE_EXPR_VAR) diag_error(node->diag_loc, "cannot reference a non-variable");
        return ir_type_make_pointer(type_operand);
    }
    if(ir_type_is_kind(type_operand, IR_TYPE_KIND_POINTER)) diag_error(node->diag_loc, "pointer type in unary expression");
    return type_operand;
}

static ir_type_t *check_expr_var(semantics_context_t *ctx, ir_node_t *node) {
    scope_var_t *variable = scope_get_variable(ctx->scope, node->expr_var.name);
    if(variable == NULL) diag_error(node->diag_loc, "reference to an undefined variable `%s`", node->expr_var.name);
    return variable->type;
}

static ir_type_t *check_expr_call(semantics_context_t *ctx, ir_node_t *node) {
    function_t *function = ctx_get_function(ctx, node->expr_call.name);
    if(function == NULL) diag_error(node->diag_loc, "reference to an undefined function `%s`", node->expr_call.name);
    if(!function->varargs && function->argument_count != node->expr_call.argument_count) diag_error(node->diag_loc, "invalid number of arguments");
    for(size_t i = 0; i < node->expr_call.argument_count; i++) {
        ir_type_t *type_argument = check_common(ctx, node->expr_call.arguments[i]);
        if(function->argument_count <= i) continue;
        node->expr_call.arguments[i] = implicit_cast(node->expr_call.arguments[i], type_argument, function->arguments[i].type);
    }
    return function->return_type;
}

static void check_stmt_block(semantics_context_t *ctx, ir_node_t *node) {
    ctx->scope = scope_make(ctx->scope, SCOPE_TYPE_BLOCK);
    for(size_t i = 0; i < node->stmt_block.statement_count; i++) check_common(ctx, node->stmt_block.statements[i]);
    ctx->scope = scope_free(ctx->scope);
}

static void check_stmt_return(semantics_context_t *ctx, ir_node_t *node) {
    if(node->stmt_return.value != NULL) check_common(ctx, node->stmt_return.value);
}

static void check_stmt_if(semantics_context_t *ctx, ir_node_t *node) {
    check_common(ctx, node->stmt_if.condition);
    check_common(ctx, node->stmt_if.body);
    if(node->stmt_if.else_body != NULL) check_common(ctx, node->stmt_if.else_body);
}

static void check_stmt_while(semantics_context_t *ctx, ir_node_t *node) {
    if(node->stmt_while.condition != NULL) check_common(ctx, node->stmt_while.condition);
    check_common(ctx, node->stmt_while.body);
}

static void check_stmt_decl(semantics_context_t *ctx, ir_node_t *node) {
    if(scope_get_variable(ctx->scope, node->stmt_decl.name)) diag_error(node->diag_loc, "redeclaration of `%s`", node->stmt_decl.name);
    if(ir_type_is_void(node->stmt_decl.type)) diag_error(node->diag_loc, "cannot declare a variable as void");
    if(node->stmt_decl.initial != NULL) {
        ir_type_t *initial_type = check_common(ctx, node->stmt_decl.initial);
        node->stmt_decl.initial = implicit_cast(node->stmt_decl.initial, initial_type, node->stmt_decl.type);
    }
    scope_add_variable(ctx->scope, node->stmt_decl.name, node->stmt_decl.type);
}

static ir_type_t *check_common(semantics_context_t *ctx, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_PROGRAM: check_program(ctx, node); break;

        case IR_NODE_TYPE_GLOBAL_FUNCTION: check_global_function(ctx, node); break;
        case IR_NODE_TYPE_GLOBAL_EXTERN: check_global_extern(ctx, node); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: return check_expr_literal_numeric(ctx, node);
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: return check_expr_literal_string(ctx, node);
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: return check_expr_literal_char(ctx, node);
        case IR_NODE_TYPE_EXPR_BINARY: return check_expr_binary(ctx, node);
        case IR_NODE_TYPE_EXPR_UNARY: return check_expr_unary(ctx, node);
        case IR_NODE_TYPE_EXPR_VAR: return check_expr_var(ctx, node);
        case IR_NODE_TYPE_EXPR_CALL: return check_expr_call(ctx, node);
        case IR_NODE_TYPE_EXPR_CAST: assert(false);

        case IR_NODE_TYPE_STMT_BLOCK: check_stmt_block(ctx, node); break;
        case IR_NODE_TYPE_STMT_RETURN: check_stmt_return(ctx, node); break;
        case IR_NODE_TYPE_STMT_IF: check_stmt_if(ctx, node); break;
        case IR_NODE_TYPE_STMT_WHILE: check_stmt_while(ctx, node); break;
        case IR_NODE_TYPE_STMT_DECL: check_stmt_decl(ctx, node); break;
    }
    return NULL;
}

void semantics_validate(ir_node_t *ast) {
    semantics_context_t ctx = {
        .scope = NULL,
        .function_count = 0,
        .functions = NULL
    };
    check_common(&ctx, ast);
}