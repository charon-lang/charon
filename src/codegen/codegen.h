#pragma once

#include "llir/node.h"
#include "llir/namespace.h"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

void codegen_ir(llir_node_t *root_node, llir_namespace_t *root_namespace, llir_type_cache_t *anon_type_cache, const char *path);
void codegen(llir_node_t *root_node, llir_namespace_t *root_namespace, llir_type_cache_t *anon_type_cache, const char *path, const char *passes, LLVMCodeModel code_model);