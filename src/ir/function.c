#include "function.h"

#include <stdlib.h>

ir_function_t *ir_function_make(const char *name, ir_type_t *return_type) {
    ir_function_t *function = malloc(sizeof(ir_function_t));
    function->name = name;
    function->return_type = return_type;
    function->argument_count = 0;
    function->arguments = NULL;
    function->varargs = false;
    return function;
}

void ir_function_add_argument(ir_function_t *function, const char *name, ir_type_t *type) {
    function->arguments = realloc(function->arguments, ++function->argument_count * sizeof(ir_function_argument_t));
    function->arguments[function->argument_count - 1] = (ir_function_argument_t) { .name = name, .type = type };
}

void ir_function_set_varargs(ir_function_t *function, bool is_varargs) {
    function->varargs = is_varargs;
}