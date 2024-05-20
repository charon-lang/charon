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
    ir_type_t *return_type;
    size_t argument_count;
    ir_type_t **arguments;
    bool varargs;
} gen_function_type_t;

typedef struct {
    gen_function_type_t type;
    const char *name;
    LLVMTypeRef llvm_type;
    LLVMValueRef value;
} gen_function_t;

typedef struct {
    ir_type_t *return_type;
    bool has_return;
} gen_current_function_t;

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
    size_t function_count;
    gen_function_t *functions;
    gen_current_function_t *current_function;
} gen_context_t;

gen_scope_t *gen_scope_enter(gen_scope_t *current);
gen_scope_t *gen_scope_exit(gen_scope_t *scope);

gen_variable_t *gen_scope_add_variable(gen_scope_t *scope, ir_type_t *type, const char *name, LLVMValueRef value);
gen_variable_t *gen_scope_get_variable(gen_scope_t *scope, const char *name);

gen_function_t *gen_add_function(gen_context_t *ctx, gen_function_t function);
gen_function_t *gen_get_function(gen_context_t *ctx, const char *name);

void gen_enter_function(gen_context_t *ctx, ir_type_t *return_type);
gen_current_function_t *gen_current_function(gen_context_t *ctx);
void gen_exit_function(gen_context_t *ctx);

LLVMTypeRef gen_llvm_type(gen_context_t *ctx, ir_type_t *type);

gen_value_t gen_expr(gen_context_t *ctx, ir_node_t *node, ir_type_t *type_expected);
void gen_stmt(gen_context_t *ctx, ir_node_t *node);
void gen_global(gen_context_t *ctx, ir_node_t *node);

void gen(ir_node_t *ast, const char *dest, const char *passes);