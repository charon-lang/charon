// #pragma once
// #include <stddef.h>
// #include "../ir/type.h"

// typedef struct {
//     const char *name;
//     ir_type_t *type;
// } scope_var_t;

// typedef enum {
//     SCOPE_TYPE_BLOCK,
//     SCOPE_TYPE_FUNCTION
// } scope_type_t;

// typedef struct scope {
//     struct scope *outer;
//     scope_type_t type;
//     size_t variable_count;
//     scope_var_t *variables;
// } scope_t;

// scope_t *scope_make(scope_t *outer, scope_type_t type);
// scope_t *scope_free(scope_t *scope);

// void scope_add_variable(scope_t *scope, const char *name, ir_type_t *type);
// scope_var_t *scope_get_variable(scope_t *scope, const char *name);