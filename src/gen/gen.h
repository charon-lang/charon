#pragma once
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include "../ir/node.h"
#include "../ir/type.h"
#include "../diag.h"

typedef struct {
    ir_type_t *type;
    LLVMValueRef value;
} gen_value_t;

typedef struct {
    ir_type_t *type;
    const char *name;
    LLVMValueRef value;
} gen_variable_t;

typedef struct gen_scope {
    struct gen_scope *outer;
    size_t variable_count;
    gen_variable_t *variables;
} gen_scope_t;

typedef struct {
    LLVMBuilderRef builder;
    LLVMContextRef context;
    LLVMModuleRef module;
    struct {
        LLVMTypeRef void_;
        LLVMTypeRef int1;
        LLVMTypeRef int8;
        LLVMTypeRef int16;
        LLVMTypeRef int32;
        LLVMTypeRef int64;
        LLVMTypeRef pointer;
    } types;
    gen_scope_t *scope;
} gen_context_t;

gen_scope_t *gen_scope_make(gen_scope_t *current);
gen_scope_t *gen_scope_free(gen_scope_t *scope);
void gen_scope_add_variable(gen_scope_t *scope, ir_type_t *type, const char *name, LLVMValueRef value);
gen_variable_t *gen_scope_get_variable(gen_scope_t *scope, const char *name);

LLVMTypeRef gen_llvm_type(gen_context_t *ctx, ir_type_t *type);

gen_value_t gen_expr(gen_context_t *ctx, ir_node_t *node);
void gen_stmt(gen_context_t *ctx, ir_node_t *node);
void gen_global(gen_context_t *ctx, ir_node_t *node);

void gen(ir_node_t *ast, const char *dest, const char *passes);