#pragma once

#include "ir/ir.h"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

void codegen_ir(ir_unit_t *unit, ir_type_cache_t *type_cache, const char *path);
void codegen(ir_unit_t *unit, ir_type_cache_t *type_cache, const char *path, const char *passes, LLVMCodeModel code_model);