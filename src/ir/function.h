#pragma once

#include "ir/type.h"

typedef struct {
    const char *name;
    ir_type_t *type;
} ir_function_argument_t;

typedef struct {
    const char *name;
    ir_type_t *return_type;
    size_t argument_count;
    ir_function_argument_t *arguments;
    bool varargs;
} ir_function_t;

ir_function_t *ir_function_make(const char *name, ir_type_t *return_type);
void ir_function_add_argument(ir_function_t *function, const char *name, ir_type_t *type);
void ir_function_set_varargs(ir_function_t *function, bool is_varargs);