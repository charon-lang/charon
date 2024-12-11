#pragma once

#include "ir/ir.h"

typedef struct {
    void (* module)(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module);
    void (* function)(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_function_t *function);
    void (* global_variable)(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_variable_t *variable);
    void (* enumeration)(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_enumeration_t *enumeration);

    void (* stmt)(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_function_t *fn, ir_scope_t *scope, ir_stmt_t *stmt);
    void (* expr)(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_function_t *fn, ir_scope_t *scope, ir_expr_t *expr);
} visitor_t;

void visit(ir_unit_t *unit, ir_type_cache_t *type_cache, visitor_t *visitor);