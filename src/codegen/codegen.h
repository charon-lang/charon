#pragma once

#include "ir/node.h"

#include <llvm-c/Core.h>
#include <llvm-c/Transforms/PassBuilder.h>

void codegen(ir_node_t *node, const char *dest, const char *passes);