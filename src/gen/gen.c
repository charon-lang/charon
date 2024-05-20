#include "gen.h"

gen_function_t *gen_add_function(gen_context_t *ctx, gen_function_t function) {
    ctx->functions = realloc(ctx->functions, sizeof(gen_function_t) * ++ctx->function_count);
    ctx->functions[ctx->function_count - 1] = function;
    return &ctx->functions[ctx->function_count - 1];
}

gen_function_t *gen_get_function(gen_context_t *ctx, const char *name) {
    for(size_t i = 0; i < ctx->function_count; i++) {
        if(strcmp(name, ctx->functions[i].name) != 0) continue;
        return &ctx->functions[i];
    }
    return NULL;
}

void gen_enter_function(gen_context_t *ctx, ir_type_t *return_type) {
    ctx->current_function = malloc(sizeof(gen_current_function_t));
    ctx->current_function->has_return = false;
    ctx->current_function->return_type = return_type;
}

gen_current_function_t *gen_current_function(gen_context_t *ctx) {
    return ctx->current_function;
}

void gen_exit_function(gen_context_t *ctx) {
    free(ctx->current_function);
    ctx->current_function = NULL;
}

LLVMTypeRef gen_llvm_type(gen_context_t *ctx, ir_type_t *type) {
    if(ir_type_is_kind(type, IR_TYPE_KIND_VOID)) return ctx->types.void_;
    if(ir_type_is_kind(type, IR_TYPE_KIND_POINTER)) return ctx->types.pointer;
    if(ir_type_is_kind(type, IR_TYPE_KIND_INTEGER)) {
        switch(type->integer.bit_size) {
            case 1: return ctx->types.int1;
            case 8: return ctx->types.int8;
            case 16: return ctx->types.int16;
            case 32: return ctx->types.int32;
            case 64: return ctx->types.int64;
        }
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
    ctx.types.int1 = LLVMInt1TypeInContext(ctx.context);
    ctx.types.int8 = LLVMInt8TypeInContext(ctx.context);
    ctx.types.int16 = LLVMInt16TypeInContext(ctx.context);
    ctx.types.int32 = LLVMInt32TypeInContext(ctx.context);
    ctx.types.int64 = LLVMInt64TypeInContext(ctx.context);
    ctx.scope = NULL;

    assert(ast->type == IR_NODE_TYPE_PROGRAM);
    for(size_t i = 0; i < ast->program.global_count; i++) gen_global(&ctx, ast->program.globals[i]);

    ctx.current_function = NULL;

    LLVMRunPasses(ctx.module, passes, NULL, LLVMCreatePassBuilderOptions());
    LLVMPrintModuleToFile(ctx.module, dest, NULL);

    // Cleanup
    LLVMDisposeBuilder(ctx.builder);
    LLVMDisposeModule(ctx.module);
    LLVMContextDispose(ctx.context);
}