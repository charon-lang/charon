#include "gen.h"

static void gen_stmt_block(gen_context_t *ctx, ir_node_t *node) {
    ctx->scope = gen_scope_make(ctx->scope);
    for(size_t i = 0; i < node->stmt_block.statement_count; i++) gen_stmt(ctx, node->stmt_block.statements[i]);
    ctx->scope = gen_scope_free(ctx->scope);
}

static void gen_stmt_return(gen_context_t *ctx, ir_node_t *node) {
    if(node->stmt_return.value == NULL) {
        LLVMBuildRetVoid(ctx->builder);
        return;
    }
    LLVMBuildRet(ctx->builder, gen_expr(ctx, node->stmt_return.value).value);
}

static void gen_stmt_if(gen_context_t *ctx, ir_node_t *node) {
    gen_value_t condition_value = gen_expr(ctx, node->stmt_if.condition);
    LLVMValueRef condition = LLVMBuildICmp(ctx->builder, LLVMIntNE, condition_value.value, LLVMConstInt(gen_llvm_type(ctx, condition_value.type), 0, false), "");

    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMBasicBlockRef bb_then = LLVMAppendBasicBlockInContext(ctx->context, func, "if.then");
    LLVMBasicBlockRef bb_else = LLVMCreateBasicBlockInContext(ctx->context, "if.else");
    LLVMBasicBlockRef bb_end = LLVMCreateBasicBlockInContext(ctx->context, "if.end");

    bool create_end = node->stmt_if.else_body == NULL;
    LLVMBuildCondBr(ctx->builder, condition, bb_then, !create_end ? bb_else : bb_end);

    // Create then, aka body
    LLVMPositionBuilderAtEnd(ctx->builder, bb_then);
    gen_stmt(ctx, node->stmt_if.body);
    if(LLVMGetBasicBlockTerminator(bb_then) == NULL) {
        LLVMBuildBr(ctx->builder, bb_end);
        create_end = true;
    }

    // Create else body
    if(node->stmt_if.else_body != NULL) {
        LLVMAppendExistingBasicBlock(func, bb_else);
        LLVMPositionBuilderAtEnd(ctx->builder, bb_else);
        gen_stmt(ctx, node->stmt_if.else_body);
        if(LLVMGetBasicBlockTerminator(bb_else) == NULL) {
            LLVMBuildBr(ctx->builder, bb_end);
            create_end = true;
        }
    }

    // Setup end block
    if(create_end) {
        LLVMAppendExistingBasicBlock(func, bb_end);
        LLVMPositionBuilderAtEnd(ctx->builder, bb_end);
    }
}

static void gen_stmt_while(gen_context_t *ctx, ir_node_t *node) {
    bool has_condition = node->stmt_while.condition != NULL;

    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMBasicBlockRef bb_condition = NULL;
    if(has_condition) bb_condition = LLVMAppendBasicBlockInContext(ctx->context, func, "loop.condition");
    LLVMBasicBlockRef bb_body = LLVMCreateBasicBlockInContext(ctx->context, "loop.body");
    LLVMBasicBlockRef bb_out = LLVMCreateBasicBlockInContext(ctx->context, "loop.out");

    LLVMBasicBlockRef bb_top = has_condition ? bb_condition : bb_body;
    LLVMBuildBr(ctx->builder, bb_top);
    if(has_condition) {
        LLVMPositionBuilderAtEnd(ctx->builder, bb_condition);
        gen_value_t condition = gen_expr(ctx, node->stmt_while.condition);
        LLVMBuildCondBr(ctx->builder, condition.value, bb_body, bb_out);
    }

    LLVMAppendExistingBasicBlock(func, bb_body);
    LLVMPositionBuilderAtEnd(ctx->builder, bb_body);
    gen_stmt(ctx, node->stmt_while.body);
    LLVMBuildBr(ctx->builder, bb_top);

    if(has_condition) {
        LLVMAppendExistingBasicBlock(func, bb_out);
        LLVMPositionBuilderAtEnd(ctx->builder, bb_out);
    }
}

static void gen_stmt_decl(gen_context_t *ctx, ir_node_t *node) {
    LLVMValueRef parent_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMBuilderRef entry_builder = LLVMCreateBuilderInContext(ctx->context);
    LLVMBasicBlockRef bb_entry = LLVMGetEntryBasicBlock(parent_func);
    LLVMPositionBuilder(entry_builder, bb_entry, LLVMGetFirstInstruction(bb_entry));
    LLVMValueRef value = LLVMBuildAlloca(entry_builder, gen_llvm_type(ctx, node->stmt_decl.type), node->stmt_decl.name);
    LLVMDisposeBuilder(entry_builder);

    gen_scope_add_variable(ctx->scope, node->stmt_decl.type, node->stmt_decl.name, value);
    if(node->stmt_decl.initial != NULL) LLVMBuildStore(ctx->builder, gen_expr(ctx, node->stmt_decl.initial).value, value);
}

void gen_stmt(gen_context_t *ctx, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC:
        case IR_NODE_TYPE_EXPR_LITERAL_STRING:
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR:
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL:
        case IR_NODE_TYPE_EXPR_BINARY:
        case IR_NODE_TYPE_EXPR_UNARY:
        case IR_NODE_TYPE_EXPR_VAR:
        case IR_NODE_TYPE_EXPR_CALL:
        case IR_NODE_TYPE_EXPR_CAST:
            gen_expr(ctx, node);
            break;

        case IR_NODE_TYPE_STMT_BLOCK: gen_stmt_block(ctx, node); break;
        case IR_NODE_TYPE_STMT_RETURN: gen_stmt_return(ctx, node); break;
        case IR_NODE_TYPE_STMT_IF: gen_stmt_if(ctx, node); break;
        case IR_NODE_TYPE_STMT_WHILE: gen_stmt_while(ctx, node); break;
        case IR_NODE_TYPE_STMT_DECL: gen_stmt_decl(ctx, node); break;
        default: assert(false);
    }
}