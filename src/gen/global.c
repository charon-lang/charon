#include "gen.h"

static LLVMValueRef add_function(gen_context_t *ctx, ir_function_decl_t *decl, diag_loc_t diag_loc) {
    if(gen_get_function(ctx, decl->name) != NULL) diag_error(diag_loc, "redefinition of '%s'", decl->name);

    size_t argument_count = decl->argument_count;
    gen_function_argument_t *arguments = malloc(sizeof(gen_function_argument_t) * argument_count);
    for(size_t i = 0; i < argument_count; i++) {
        arguments[i] = (gen_function_argument_t) {
            .name = decl->arguments[i].name,
            .type = decl->arguments[i].type
        };
    }
    gen_add_function(ctx, decl->name, decl->return_type, argument_count, arguments, decl->varargs);

    LLVMTypeRef args[decl->argument_count];
    for(size_t i = 0; i < decl->argument_count; i++) args[i] = gen_llvm_type(ctx, decl->arguments[i].type);
    return LLVMAddFunction(ctx->module, decl->name, LLVMFunctionType(gen_llvm_type(ctx, decl->return_type), args, decl->argument_count, decl->varargs));
}

static void gen_global_extern(gen_context_t *ctx, ir_node_t *node) {
    add_function(ctx, &node->global_extern.decl, node->diag_loc);
}

static void gen_global_function(gen_context_t *ctx, ir_node_t *node) {
    LLVMValueRef func = add_function(ctx, &node->global_function.decl, node->diag_loc);

    LLVMBasicBlockRef bb_entry = LLVMAppendBasicBlockInContext(ctx->context, func, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb_entry);

    ctx->scope = gen_scope_enter(ctx->scope);
    for(size_t i = 0; i < node->global_function.decl.argument_count; i++) {
        ir_type_t *param_type = node->global_function.decl.arguments[i].type;
        const char *param_name = node->global_function.decl.arguments[i].name;
        LLVMValueRef param_original = LLVMGetParam(func, i);
        LLVMValueRef param_new = LLVMBuildAlloca(ctx->builder, gen_llvm_type(ctx, param_type), param_name);
        LLVMBuildStore(ctx->builder, param_original, param_new);
        gen_scope_add_variable(ctx->scope, param_type, param_name, param_new);
    }
    gen_stmt(ctx, node->global_function.body);
    ctx->scope = gen_scope_exit(ctx->scope);
}

void gen_global(gen_context_t *ctx, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_GLOBAL_FUNCTION: gen_global_function(ctx, node); return;
        case IR_NODE_TYPE_GLOBAL_EXTERN: gen_global_extern(ctx, node); return;
        default: assert(false);
    }
}