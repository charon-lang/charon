#pragma once

#include "ir/ir.h"
#include "ast/node.h"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

typedef struct pass_lower_context pass_lower_context_t;

pass_lower_context_t *pass_lower_context_make(memory_allocator_t *work_allocator, ir_unit_t *unit, ir_type_cache_t *type_cache);
void pass_lower_populate_final(pass_lower_context_t *context, ast_node_t *root_node);
void pass_lower_populate_types(pass_lower_context_t *context, ast_node_t *root_node);
void pass_lower_populate_modules(pass_lower_context_t *context, ast_node_t *root_node);
void pass_lower(pass_lower_context_t *context, ast_node_t *root_node);

void pass_eval_types(ir_unit_t *unit, ir_type_cache_t *type_cache);

void pass_codegen_ir(ir_unit_t *unit, ir_type_cache_t *type_cache, const char *path);
void pass_codegen(ir_unit_t *unit, ir_type_cache_t *type_cache, const char *path, const char *passes, LLVMCodeModel code_model);