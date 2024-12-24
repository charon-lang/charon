#pragma once

#include "ir/ir.h"

typedef enum {
    CODEGEN_CODE_MODEL_DEFAULT,
    CODEGEN_CODE_MODEL_TINY,
    CODEGEN_CODE_MODEL_SMALL,
    CODEGEN_CODE_MODEL_MEDIUM,
    CODEGEN_CODE_MODEL_LARGE,
    CODEGEN_CODE_MODEL_KERNEL
} codegen_code_model_t;

typedef enum {
    CODEGEN_OPTIMIZATION_NONE,
    CODEGEN_OPTIMIZATION_O1,
    CODEGEN_OPTIMIZATION_O2,
    CODEGEN_OPTIMIZATION_O3
} codegen_optimization_t;

typedef struct codegen_context codegen_context_t;

codegen_context_t *codegen(ir_unit_t *unit, ir_type_cache_t *type_cache, codegen_optimization_t optimization, codegen_code_model_t code_model);
void codegen_emit(codegen_context_t *context, const char *path, bool ir);
int codegen_run(codegen_context_t *context);
void codegen_dispose_context(codegen_context_t *context);