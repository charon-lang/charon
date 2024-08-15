#pragma once

#include "ir/node.h"

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

void codegen(ir_node_t *node, const char *path, const char *passes);
void codegen_ir(ir_node_t *node, const char *path);