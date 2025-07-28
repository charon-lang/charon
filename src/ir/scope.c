#include "ir.h"
#include "lib/alloc.h"

#include <assert.h>
#include <string.h>

ir_variable_t *ir_scope_add(ir_scope_t *scope, const char *name, ir_type_t *type, ir_expr_t *initial_value) {
    ir_variable_t *variable = alloc(sizeof(ir_variable_t));
    variable->name = name;
    variable->type = type;
    variable->initial_value = initial_value;
    variable->is_local = true;
    variable->scope = scope;
    variable->codegen_data = NULL;

    scope->variables = alloc_array(scope->variables, ++scope->variable_count, sizeof(ir_variable_t *));
    scope->variables[scope->variable_count - 1] = variable;
    return variable;
}

ir_variable_t *ir_scope_find_variable(ir_scope_t *scope, const char *name) {
    assert(scope != NULL);
    for(size_t i = 0; i < scope->variable_count; i++) {
        if(strcmp(name, scope->variables[i]->name) != 0) continue;
        return scope->variables[i];
    }
    if(scope->parent == NULL) return NULL;
    return ir_scope_find_variable(scope->parent, name);
}
