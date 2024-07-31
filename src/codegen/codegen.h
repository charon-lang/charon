#pragma once

#include "ir/node.h"

void codegen(ir_node_t *node, const char *path, const char *passes);
void codegen_ir(ir_node_t *node, const char *path);