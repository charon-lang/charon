#include "gen.h"

gen_scope_t *gen_scope_make(gen_scope_t *current) {
    gen_scope_t *scope = malloc(sizeof(gen_scope_t));
    scope->outer = current;
    scope->variable_count = 0;
    scope->variables = NULL;
    return scope;
}

gen_scope_t *gen_scope_free(gen_scope_t *scope) {
    gen_scope_t *new = scope->outer;
    free(scope->variables);
    free(scope);
    return new;
}

void gen_scope_add_variable(gen_scope_t *scope, ir_type_t *type, const char *name, LLVMValueRef value) {
    assert(scope != NULL);
    scope->variables = realloc(scope->variables, sizeof(gen_variable_t) * ++scope->variable_count);
    scope->variables[scope->variable_count - 1] = (gen_variable_t) { .type = type, .name = name, .value = value };
}

gen_variable_t *gen_scope_get_variable(gen_scope_t *scope, const char *name) {
    assert(scope != NULL);
    for(size_t i = 0; i < scope->variable_count; i++) {
        if(strcmp(name, scope->variables[i].name) != 0) continue;
        return &scope->variables[i];
    }
    return gen_scope_get_variable(scope->outer, name);
}