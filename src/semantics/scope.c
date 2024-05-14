#include "scope.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

scope_t *scope_make(scope_t *outer, scope_type_t type) {
    scope_t *scope = malloc(sizeof(scope_t));
    scope->outer = outer;
    scope->type = type;
    scope->variable_count = 0;
    scope->variables = NULL;
    return scope;
}

scope_t *scope_free(scope_t *scope) {
    assert(scope != NULL);
    scope_t *outer = scope->outer;
    if(scope->variables) free(scope->variables);
    free(scope);
    return outer;
}

void scope_add_variable(scope_t *scope, const char *name, ir_type_t *type) {
    assert(scope != NULL);
    scope->variables = realloc(scope->variables, sizeof(scope_var_t) * ++scope->variable_count);
    scope->variables[scope->variable_count - 1] = (scope_var_t) { .name = name, .type = type };
}

scope_var_t *scope_get_variable(scope_t *scope, const char *name) {
    if(scope == NULL) return NULL;
    for(size_t i = 0; i < scope->variable_count; i++) {
        if(strcmp(scope->variables[i].name, name) != 0) continue;
        return &scope->variables[i];
    }
    return scope_get_variable(scope->outer, name);
}