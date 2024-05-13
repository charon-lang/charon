#include "gen.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include "../diag.h"
#include "../ir/type.h"

typedef struct {
    const char *name;
    LLVMValueRef value;
} variable_t;

typedef struct scope {
    struct scope *outer;
    size_t variable_count;
    variable_t *variables;
} scope_t;

typedef struct {
    LLVMBuilderRef builder;
    LLVMContextRef context;
    LLVMModuleRef module;
    struct {
        LLVMTypeRef void_;
        LLVMTypeRef int8;
        LLVMTypeRef int16;
        LLVMTypeRef int32;
        LLVMTypeRef int64;
        LLVMTypeRef pointer;
    } types;
    scope_t *scope;
} gen_context_t;

static scope_t *scope_make(scope_t *current) {
    scope_t *scope = malloc(sizeof(scope_t));
    scope->outer = current;
    scope->variable_count = 0;
    scope->variables = NULL;
    return scope;
}

static scope_t *scope_free(scope_t *scope) {
    scope_t *new = scope->outer;
    free(scope->variables);
    free(scope);
    return new;
}

static void scope_add_variable(scope_t *scope, LLVMValueRef value, const char *name, diag_loc_t diag_loc) {
    if(scope == NULL) diag_error(diag_loc, "new variable outside of scope");
    scope->variables = realloc(scope->variables, sizeof(variable_t) * ++scope->variable_count);
    scope->variables[scope->variable_count - 1] = (variable_t) { .name = name, .value = value };
}

static variable_t *scope_get_variable(scope_t *scope, const char *name, diag_loc_t diag_loc) {
    if(scope == NULL) diag_error(diag_loc, "cannot get variable outside of scope");
    for(size_t i = 0; i < scope->variable_count; i++) {
        if(strcmp(name, scope->variables[i].name) == 0) return &scope->variables[i];
    }
    if(scope->outer == NULL) return NULL;
    return scope_get_variable(scope->outer, name, diag_loc);
}

static LLVMTypeRef get_llvm_type(gen_context_t *ctx, ir_type_t *type, diag_loc_t diag_loc) {
    if(ir_type_is_kind(type, IR_TYPE_KIND_VOID)) return ctx->types.void_;
    if(ir_type_is_kind(type, IR_TYPE_KIND_POINTER)) return ctx->types.pointer;
    if(ir_type_is_kind(type, IR_TYPE_KIND_INTEGER)) {
        switch(type->integer.bit_size) {
            case 8: return ctx->types.int8;
            case 16: return ctx->types.int16;
            case 32: return ctx->types.int32;
            case 64: return ctx->types.int64;
        }
    }
    diag_error(diag_loc, "unexpected type");
}

static LLVMValueRef add_function(gen_context_t *ctx, ir_function_decl_t *decl, diag_loc_t diag_loc) {
    LLVMTypeRef args[decl->argument_count];
    for(size_t i = 0; i < decl->argument_count; i++) {
        args[i] = get_llvm_type(ctx, decl->arguments[i].type, decl->arguments[i].diag_loc);
    }
    LLVMTypeRef return_type = get_llvm_type(ctx, decl->return_type, diag_loc);
    return LLVMAddFunction(
        ctx->module,
        decl->name,
        LLVMFunctionType(return_type, args, decl->argument_count, decl->varargs)
    );
}

static LLVMValueRef gen_common(gen_context_t *ctx, ir_node_t *node);

static LLVMValueRef gen_program(gen_context_t *ctx, ir_node_t *node) {
    for(size_t i = 0; i < node->program.global_count; i++) gen_common(ctx, node->program.globals[i]);
    return NULL;
}

static LLVMValueRef gen_global_extern(gen_context_t *ctx, ir_node_t *node) {
    add_function(ctx, &node->global_extern.decl, node->diag_loc);
    return NULL;
}

static LLVMValueRef gen_global_function(gen_context_t *ctx, ir_node_t *node) {
    LLVMValueRef func = add_function(ctx, &node->global_function.decl, node->diag_loc);

    LLVMBasicBlockRef bb_entry = LLVMAppendBasicBlockInContext(ctx->context, func, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb_entry);

    ctx->scope = scope_make(ctx->scope);
    for(size_t i = 0; i < node->global_function.decl.argument_count; i++) {
        const char *param_name = node->global_function.decl.arguments[i].name;
        LLVMValueRef param_original = LLVMGetParam(func, i);
        LLVMValueRef param_new = LLVMBuildAlloca(ctx->builder, LLVMTypeOf(param_original), param_name);
        LLVMBuildStore(ctx->builder, param_original, param_new);
        scope_add_variable(ctx->scope, param_new, param_name, node->global_function.decl.arguments[i].diag_loc);
    }
    gen_common(ctx, node->global_function.body);
    ctx->scope = scope_free(ctx->scope);
    return NULL;
}

static LLVMValueRef gen_expr_literal_numeric(gen_context_t *ctx, ir_node_t *node) {
    return LLVMConstInt(ctx->types.int64, node->expr_literal.numeric_value, false);
}

static LLVMValueRef gen_expr_literal_string(gen_context_t *ctx, ir_node_t *node) {
    return LLVMBuildGlobalString(ctx->builder, node->expr_literal.string_value, ""); // TODO: Constant string?
}

static LLVMValueRef gen_expr_literal_char(gen_context_t *ctx, ir_node_t *node) {
    return LLVMConstInt(ctx->types.int8, node->expr_literal.char_value, false);
}

static LLVMValueRef gen_expr_binary_operation(gen_context_t *ctx, ir_node_t *node) {
    LLVMValueRef right = gen_common(ctx, node->expr_binary.right);

    // TODO: re-investigate this generation. its a bit scuffed imo.
    if(node->expr_binary.operation == IR_BINARY_OPERATION_ASSIGN) {
        assert(node->expr_binary.left->type == IR_NODE_TYPE_EXPR_VAR);

        const char *name = node->expr_binary.left->expr_var.name;
        variable_t *var = scope_get_variable(ctx->scope, name, node->diag_loc);
        if(var == NULL) diag_error(node->diag_loc, "reference to an undefined variable `%s`", name);

        LLVMBuildStore(ctx->builder, right, var->value);
        return right;
    }

    LLVMValueRef left = gen_common(ctx, node->expr_binary.left);
    switch(node->expr_binary.operation) {
        case IR_BINARY_OPERATION_ADDITION: return LLVMBuildAdd(ctx->builder, left, right, "");
        case IR_BINARY_OPERATION_SUBTRACTION: return LLVMBuildSub(ctx->builder, left, right, "");
        case IR_BINARY_OPERATION_MULTIPLICATION: return LLVMBuildMul(ctx->builder, left, right, "");
        case IR_BINARY_OPERATION_DIVISION: return LLVMBuildUDiv(ctx->builder, left, right, "");
        case IR_BINARY_OPERATION_MODULO: return LLVMBuildSRem(ctx->builder, left, right, "");
        case IR_BINARY_OPERATION_EQUAL: return LLVMBuildICmp(ctx->builder, LLVMIntEQ, left, right, "");
        case IR_BINARY_OPERATION_NOT_EQUAL: return LLVMBuildICmp(ctx->builder, LLVMIntNE, left, right, "");
        case IR_BINARY_OPERATION_GREATER: return LLVMBuildICmp(ctx->builder, LLVMIntUGT, left, right, "");
        case IR_BINARY_OPERATION_GREATER_EQUAL: return LLVMBuildICmp(ctx->builder, LLVMIntUGE, left, right, "");
        case IR_BINARY_OPERATION_LESS: return LLVMBuildICmp(ctx->builder, LLVMIntULT, left, right, "");
        case IR_BINARY_OPERATION_LESS_EQUAL: return LLVMBuildICmp(ctx->builder, LLVMIntULE, left, right, "");
        default: diag_error(node->diag_loc, "unimplemented binary operation %i", node->expr_binary.operation); break;
    }
    __builtin_unreachable();
}

static LLVMValueRef gen_expr_var(gen_context_t *ctx, ir_node_t *node) {
    variable_t *var = scope_get_variable(ctx->scope, node->expr_var.name, node->diag_loc);
    if(var == NULL) diag_error(node->diag_loc, "reference to an undefined local `%s`", node->expr_var.name);
    return LLVMBuildLoad2(ctx->builder, LLVMGetAllocatedType(var->value), var->value, "");
}

static LLVMValueRef gen_expr_call(gen_context_t *ctx, ir_node_t *node) {
    LLVMValueRef func_ref = LLVMGetNamedFunction(ctx->module, node->expr_call.name);
    if(func_ref != NULL) {
        LLVMValueRef args[node->expr_call.argument_count];
        for(size_t i = 0; i < node->expr_call.argument_count; i++) args[i] = gen_common(ctx, node->expr_call.arguments[i]);
        return LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(func_ref), func_ref, args, node->expr_call.argument_count, "");
    }
    diag_error(node->diag_loc, "reference to an undefined function `%s`", node->expr_call.name);
    __builtin_unreachable();
}

static LLVMValueRef gen_expr_cast(gen_context_t *ctx, ir_node_t *node) {
    LLVMTypeRef to_type = get_llvm_type(ctx, node->expr_cast.type, node->diag_loc);
    LLVMValueRef value = gen_common(ctx, node->expr_cast.value);
    LLVMTypeRef type = LLVMTypeOf(value);
    if(LLVMGetTypeKind(to_type) != LLVMGetTypeKind(type)) diag_error(node->diag_loc, "cast of incompatible types");

    LLVMTargetDataRef data_layout = LLVMGetModuleDataLayout(ctx->module);
    unsigned long long to_size = LLVMSizeOfTypeInBits(data_layout, to_type);
    unsigned long long size = LLVMSizeOfTypeInBits(data_layout, type);
    if(size > to_size) return LLVMBuildTruncOrBitCast(ctx->builder, value, to_type, "");
    if(to_size > size) return LLVMBuildZExtOrBitCast(ctx->builder, value, to_type, "");
    return value;
}

static LLVMValueRef gen_stmt_block(gen_context_t *ctx, ir_node_t *node) {
    ctx->scope = scope_make(ctx->scope);
    for(size_t i = 0; i < node->stmt_block.statement_count; i++) gen_common(ctx, node->stmt_block.statements[i]);
    ctx->scope = scope_free(ctx->scope);
    return NULL;
}

static LLVMValueRef gen_stmt_return(gen_context_t *ctx, ir_node_t *node) {
    if(node->stmt_return.value == NULL) {
        LLVMBuildRetVoid(ctx->builder);
    } else {
        LLVMBuildRet(ctx->builder, gen_common(ctx, node->stmt_return.value));
    }
    return NULL;
}

static LLVMValueRef gen_common(gen_context_t *ctx, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_PROGRAM: return gen_program(ctx, node);
        case IR_NODE_TYPE_GLOBAL_FUNCTION: return gen_global_function(ctx, node);
        case IR_NODE_TYPE_GLOBAL_EXTERN: return gen_global_extern(ctx, node);

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: return gen_expr_literal_numeric(ctx, node);
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: return gen_expr_literal_string(ctx, node);
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: return gen_expr_literal_char(ctx, node);
        case IR_NODE_TYPE_EXPR_BINARY: return gen_expr_binary_operation(ctx, node);
        // case IR_NODE_TYPE_EXPR_UNARY: return check_expr_unary(node);
        case IR_NODE_TYPE_EXPR_VAR: return gen_expr_var(ctx, node);
        case IR_NODE_TYPE_EXPR_CALL: return gen_expr_call(ctx, node);
        case IR_NODE_TYPE_EXPR_CAST: return gen_expr_cast(ctx, node);

        case IR_NODE_TYPE_STMT_BLOCK: return gen_stmt_block(ctx, node);
        case IR_NODE_TYPE_STMT_RETURN: return gen_stmt_return(ctx, node);
        // case IR_NODE_TYPE_STMT_IF: return check_stmt_if(node);
        // case IR_NODE_TYPE_STMT_DECL: return check_stmt_decl(node);
        default: break;
    }
    assert(false);
}

void gen(ir_node_t *ast, const char *dest, const char *passes) {
    gen_context_t ctx = {};
    ctx.context = LLVMContextCreate();
    ctx.module = LLVMModuleCreateWithNameInContext("CharonModule", ctx.context);
    ctx.builder = LLVMCreateBuilderInContext(ctx.context);

    ctx.types.void_ = LLVMVoidTypeInContext(ctx.context);
    ctx.types.pointer = LLVMPointerTypeInContext(ctx.context, 0);
    ctx.types.int8 = LLVMInt8TypeInContext(ctx.context);
    ctx.types.int16 = LLVMInt16TypeInContext(ctx.context);
    ctx.types.int32 = LLVMInt32TypeInContext(ctx.context);
    ctx.types.int64 = LLVMInt64TypeInContext(ctx.context);
    ctx.scope = NULL;

    gen_common(&ctx, ast);

    LLVMRunPasses(ctx.module, passes, NULL, LLVMCreatePassBuilderOptions());
    LLVMPrintModuleToFile(ctx.module, dest, NULL);

    // Cleanup
    LLVMDisposeBuilder(ctx.builder);
    LLVMDisposeModule(ctx.module);
    LLVMContextDispose(ctx.context);
}