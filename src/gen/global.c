#include "gen.h"

static bool cmp_functions(gen_function_type_t *a, gen_function_type_t *b) {
    if(a->varargs != b->varargs || a->argument_count != b->argument_count) return false;
    if(!ir_type_is_eq(a->return_type, b->return_type)) return false;
    for(size_t i = 0; i < a->argument_count; i++) if(!ir_type_is_eq(a->arguments[i], b->arguments[i])) return false;
    return true;
}

static gen_function_type_t make_function_type(ir_function_decl_t *decl) {
    ir_type_t **arguments = malloc(sizeof(ir_type_t *) * decl->argument_count);
    for(size_t i = 0; i < decl->argument_count; i++) arguments[i] = decl->arguments[i].type;
    return (gen_function_type_t) {
        .return_type = decl->return_type,
        .argument_count = decl->argument_count,
        .arguments = arguments,
        .varargs = decl->varargs
    };
}

static gen_function_t *add_function(gen_context_t *ctx, const char *name, gen_function_type_t function_type) {
    LLVMTypeRef args[function_type.argument_count];
    for(size_t i = 0; i < function_type.argument_count; i++) args[i] = gen_llvm_type(ctx, function_type.arguments[i]);
    LLVMTypeRef func_type = LLVMFunctionType(gen_llvm_type(ctx, function_type.return_type), args, function_type.argument_count, function_type.varargs);
    return gen_add_function(ctx, (gen_function_t) { .name = name, .type = function_type, .value = LLVMAddFunction(ctx->module, name, func_type) });
}

static void gen_global_extern(gen_context_t *ctx, ir_node_t *node) {
    const char *func_name = node->global_extern.decl.name;
    gen_function_t *existing_func = gen_get_function(ctx, func_name);
    gen_function_type_t func_type = make_function_type(&node->global_extern.decl);
    if(existing_func != NULL && !cmp_functions(&existing_func->type, &func_type)) diag_error(node->diag_loc, "conflicting types for '%s'", func_name);
    add_function(ctx, func_name, func_type);
}

static void gen_global_function(gen_context_t *ctx, ir_node_t *node) {
    const char *func_name = node->global_function.decl.name;
    if(gen_get_function(ctx, func_name) != NULL) diag_error(node->diag_loc, "redefinition of '%s'", func_name);
    gen_function_t *func = add_function(ctx, func_name, make_function_type(&node->global_function.decl));

    LLVMBasicBlockRef bb_entry = LLVMAppendBasicBlockInContext(ctx->context, func->value, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb_entry);

    ctx->scope = gen_scope_enter(ctx->scope);
    for(size_t i = 0; i < node->global_function.decl.argument_count; i++) {
        ir_type_t *param_type = node->global_function.decl.arguments[i].type;
        const char *param_name = node->global_function.decl.arguments[i].name;
        LLVMValueRef param_original = LLVMGetParam(func->value, i);
        LLVMValueRef param_new = LLVMBuildAlloca(ctx->builder, gen_llvm_type(ctx, param_type), param_name);
        LLVMBuildStore(ctx->builder, param_original, param_new);
        gen_scope_add_variable(ctx->scope, param_type, param_name, param_new);
    }
    gen_enter_function(ctx, func->type.return_type);
    gen_stmt(ctx, node->global_function.body);
    gen_exit_function(ctx);
    ctx->scope = gen_scope_exit(ctx->scope);
}

void gen_global(gen_context_t *ctx, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_GLOBAL_FUNCTION: gen_global_function(ctx, node); return;
        case IR_NODE_TYPE_GLOBAL_EXTERN: gen_global_extern(ctx, node); return;
        default: assert(false);
    }
}