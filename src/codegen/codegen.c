#include "codegen.h"

#include "lib/log.h"
#include "lib/diag.h"
#include "codegen/symbol.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct codegen_scope codegen_scope_t;

typedef struct {
    bool iref;
    ir_type_t *type;
    LLVMValueRef value;
} codegen_value_container_t;

typedef struct {
    codegen_scope_t *scope;
    const char *name;
    ir_type_t *type;
    LLVMValueRef ref;
} codegen_variable_t;

struct codegen_scope {
    struct codegen_scope *parent;
    codegen_variable_t *variables;
    size_t variable_count;
};

typedef struct {
    LLVMModuleRef module;
    LLVMContextRef context;
    LLVMBuilderRef builder;

    symbol_t *current_module;

    symbol_list_t symbols;

    ir_type_t *current_function_return_type;
    bool current_function_returned;
} codegen_state_t;

static char *name_in_module(char *name, symbol_t *module) {
    name = strdup(name);
    while(true) {
        if(module == NULL) break;
        size_t name_length = strlen(name);
        size_t module_name_length = strlen(module->name);
        char *new = malloc(module_name_length + 2 + name_length + 1);
        memcpy(new, module->name, module_name_length);
        new[module_name_length] = ':';
        new[module_name_length + 1] = ':';
        memcpy(&new[module_name_length + 2], name, name_length);
        new[module_name_length + 2 + name_length] = '\0';

        free(name);
        name = new;
        module = module->module.parent;
    }
    return name;
}

static LLVMTypeRef llvm_type(codegen_state_t *state, ir_type_t *type) {
    assert(type != NULL);
    switch(type->kind) {
        case IR_TYPE_KIND_VOID: return LLVMVoidTypeInContext(state->context);
        case IR_TYPE_KIND_INTEGER:
            switch(type->integer.bit_size) {
                case 1: return LLVMInt1TypeInContext(state->context);
                case 8: return LLVMInt8TypeInContext(state->context);
                case 16: return LLVMInt16TypeInContext(state->context);
                case 32: return LLVMInt32TypeInContext(state->context);
                case 64: return LLVMInt64TypeInContext(state->context);
                default: assert(false);
            }
        case IR_TYPE_KIND_POINTER: return LLVMPointerTypeInContext(state->context, 0);
        case IR_TYPE_KIND_ARRAY: return LLVMArrayType2(llvm_type(state, type->array.type), type->array.size);
        case IR_TYPE_KIND_TUPLE:
            LLVMTypeRef types[type->tuple.type_count];
            for(size_t i = 0; i < type->tuple.type_count; i++) types[i] = llvm_type(state, type->tuple.types[i]);
            return LLVMStructTypeInContext(state->context, types, type->tuple.type_count, false);
    }
    assert(false);
}

static void scope_add_variable(codegen_scope_t *scope, const char *name, ir_type_t *type, LLVMValueRef ref) {
    scope->variables = realloc(scope->variables, ++scope->variable_count * sizeof(codegen_variable_t));
    scope->variables[scope->variable_count - 1] = (codegen_variable_t) { .scope = scope, .name = name, .type = type, .ref = ref };
}

static codegen_variable_t *scope_get_variable(codegen_scope_t *scope, const char *name) {
    assert(scope != NULL);
    for(size_t i = 0; i < scope->variable_count; i++) {
        if(strcmp(name, scope->variables[i].name) != 0) continue;
        return &scope->variables[i];
    }
    if(scope->parent == NULL) return NULL;
    return scope_get_variable(scope->parent, name);
}

static codegen_scope_t *scope_enter(codegen_scope_t *parent) {
    codegen_scope_t *scope = malloc(sizeof(codegen_scope_t));
    scope->variable_count = 0;
    scope->variables = NULL;
    scope->parent = parent;
    return scope;
}

static codegen_scope_t *scope_exit(codegen_scope_t *scope) {
    assert(scope != NULL);
    codegen_scope_t *parent = scope->parent;
    free(scope->variables);
    free(scope);
    return parent;
}

static symbol_t *state_add_function(codegen_state_t *state, ir_function_t *prototype, const char *full_name) {
    LLVMTypeRef fn_arguments[prototype->argument_count];
    for(size_t i = 0; i < prototype->argument_count; i++) fn_arguments[i] = llvm_type(state, prototype->arguments[i].type);
    LLVMTypeRef fn_return_type = LLVMVoidTypeInContext(state->context);
    if(prototype->return_type != NULL) fn_return_type = llvm_type(state, prototype->return_type);
    LLVMTypeRef fn_type = LLVMFunctionType(fn_return_type, fn_arguments, prototype->argument_count, prototype->varargs);
    LLVMValueRef fn = LLVMAddFunction(state->module, full_name, fn_type);
    return symbol_list_add_function(state->current_module == NULL ? &state->symbols : &state->current_module->module.symbols, prototype->name, full_name, prototype, fn_type, fn);
}

static symbol_t *state_get_function(codegen_state_t *state, const char *name, bool rec) {
    if(state->current_module != NULL) {
        symbol_t *symbol = symbol_list_find_type(&state->current_module->module.symbols, name, SYMBOL_TYPE_FUNCTION);
        if(symbol != NULL) return symbol;
        if(!rec) return NULL;
    }
    return symbol_list_find_type(&state->symbols, name, SYMBOL_TYPE_FUNCTION);
}

static codegen_value_container_t resolve_iref(codegen_state_t *state, codegen_value_container_t value) {
    if(!value.iref) return value;
    return (codegen_value_container_t) {
        .type = value.type,
        .value = LLVMBuildLoad2(state->builder, llvm_type(state, value.type), value.value, "resolve_iref")
    };
}

static void cg(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node);
static codegen_value_container_t cg_expr_ext_selected_module(codegen_state_t *state, codegen_scope_t *scope, symbol_t *selected_module, ir_node_t *node);
static codegen_value_container_t cg_expr_ext(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node, bool do_resolve_iref);
static codegen_value_container_t cg_expr(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node);

static void cg_list(codegen_state_t *state, codegen_scope_t *scope, ir_node_list_t *list) {
    ir_node_t *node = list->first;
    while(node != NULL) {
        if(state->current_function_returned) diag_error(node->source_location, "unreachable");
        cg(state, scope, node);
        node = node->next;
    }
}

static void cg_tlc_module(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    symbol_list_t *symbols = state->current_module == NULL ? &state->symbols : &state->current_module->module.symbols;
    state->current_module = symbol_list_add_module(symbols, node->tlc_module.name, state->current_module);
    cg_list(state, scope, &node->tlc_module.tlcs);
    state->current_module = state->current_module->module.parent;
}

static void cg_tlc_function(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    if(state_get_function(state, node->tlc_function.prototype->name, false) != NULL) diag_error(node->source_location, "redefinition of `%s`", node->tlc_function.prototype->name);

    char *name = (char *) node->tlc_function.prototype->name;
    if(state->current_module != NULL) name = name_in_module(name, state->current_module);
    symbol_t *symbol = state_add_function(state, node->tlc_function.prototype, name);

    LLVMBasicBlockRef bb_entry = LLVMAppendBasicBlockInContext(state->context, symbol->function.value, "tlc.function");
    LLVMPositionBuilderAtEnd(state->builder, bb_entry);

    state->current_function_return_type = node->tlc_function.prototype->return_type;
    state->current_function_returned = false;
    scope = scope_enter(scope);
    for(size_t i = 0; i < node->tlc_function.prototype->argument_count; i++) {
        ir_function_argument_t *argument = &node->tlc_function.prototype->arguments[i];
        LLVMValueRef param = LLVMBuildAlloca(state->builder, llvm_type(state, argument->type), argument->name);
        LLVMBuildStore(state->builder, LLVMGetParam(symbol->function.value, i), param);
        scope_add_variable(scope, argument->name, argument->type, param);
    }
    cg_list(state, scope, &node->tlc_function.statements);
    scope = scope_exit(scope);

    if(!state->current_function_returned) {
        if(symbol->function.prototype->return_type->kind != IR_TYPE_KIND_VOID) diag_error(node->source_location, "missing return");
        LLVMBuildRetVoid(state->builder);
    }
    state->current_function_return_type = NULL;
    state->current_function_returned = false;
}

static void cg_tlc_extern(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    if(state_get_function(state, node->tlc_extern.prototype->name, false) != NULL) diag_error(node->source_location, "redefinition of `%s`", node->tlc_function.prototype->name);
    state_add_function(state, node->tlc_extern.prototype, node->tlc_extern.prototype->name);
}

static void cg_stmt_block(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    scope = scope_enter(scope);
    cg_list(state, scope, &node->stmt_block.statements);
    scope = scope_exit(scope);
}

static void cg_stmt_declaration(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    codegen_variable_t *var = scope_get_variable(scope, node->stmt_declaration.name);
    if(var != NULL) {
        if(var->scope == scope) diag_error(node->source_location, "redeclaration of `%s`", node->stmt_declaration.name);
        diag_warn(node->source_location, "declaration shadows `%s`", node->stmt_declaration.name);
    }

    LLVMValueRef func_parent = LLVMGetBasicBlockParent(LLVMGetInsertBlock(state->builder));

    LLVMValueRef value = NULL;
    ir_type_t *type = node->stmt_declaration.type;
    if(node->stmt_declaration.initial != NULL) {
        codegen_value_container_t cg_value = cg_expr(state, scope, node->stmt_declaration.initial);
        if(type == NULL) {
            type = cg_value.type;
        } else {
            if(!ir_type_eq(cg_value.type, type)) diag_error(node->source_location, "declarations initial value does not match its explicit type");
        }
        value = cg_value.value;
    }
    if(type == NULL) diag_error(node->source_location, "declaration is missing both an explicit and inferred type");

    LLVMBasicBlockRef bb_entry = LLVMGetEntryBasicBlock(func_parent);
    LLVMBuilderRef entry_builder = LLVMCreateBuilderInContext(state->context);
    LLVMPositionBuilder(entry_builder, bb_entry, LLVMGetFirstInstruction(bb_entry));
    LLVMValueRef ref = LLVMBuildAlloca(entry_builder, llvm_type(state, type), node->stmt_declaration.name);
    LLVMDisposeBuilder(entry_builder);
    if(value != NULL) LLVMBuildStore(state->builder, value, ref);

    scope_add_variable(scope, node->stmt_declaration.name, type, ref);
}

static void cg_stmt_return(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    state->current_function_returned = true;
    if(state->current_function_return_type->kind == IR_TYPE_KIND_VOID) {
        if(node->stmt_return.value) cg_expr(state, scope, node->stmt_return.value);
        LLVMBuildRetVoid(state->builder);
        return;
    }
    if(node->stmt_return.value == NULL) diag_error(node->source_location, "no return value");
    codegen_value_container_t value = cg_expr(state, scope, node->stmt_return.value);
    if(!ir_type_eq(value.type, state->current_function_return_type)) diag_error(node->source_location, "invalid type");
    LLVMBuildRet(state->builder, value.value);
}

static void cg_stmt_if(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(state->builder));
    LLVMBasicBlockRef bb_then = LLVMAppendBasicBlockInContext(state->context, func, "if.then");
    LLVMBasicBlockRef bb_else = LLVMCreateBasicBlockInContext(state->context, "if.else");
    LLVMBasicBlockRef bb_end = LLVMCreateBasicBlockInContext(state->context, "if.end");

    codegen_value_container_t condition = cg_expr(state, scope, node->stmt_if.condition);
    if(!ir_type_eq(condition.type, ir_type_get_bool())) diag_error(node->stmt_if.condition->source_location, "condition is not a boolean");
    LLVMBuildCondBr(state->builder, condition.value, bb_then, node->stmt_if.else_body != NULL ? bb_else : bb_end);

    bool then_returned = false;
    bool else_returned = false;

    // Create then, aka body
    LLVMPositionBuilderAtEnd(state->builder, bb_then);
    cg(state, scope, node->stmt_if.body);
    then_returned = state->current_function_returned;
    state->current_function_returned = false;
    if(!then_returned) LLVMBuildBr(state->builder, bb_end);

    // Create else body
    if(node->stmt_if.else_body != NULL) {
        LLVMAppendExistingBasicBlock(func, bb_else);
        LLVMPositionBuilderAtEnd(state->builder, bb_else);
        cg(state, scope, node->stmt_if.else_body);
        else_returned = state->current_function_returned;
        state->current_function_returned = false;
        if(!else_returned) LLVMBuildBr(state->builder, bb_end);
    }
    state->current_function_returned = then_returned && else_returned;

    // Setup end block
    if(then_returned && else_returned) return;
    LLVMAppendExistingBasicBlock(func, bb_end);
    LLVMPositionBuilderAtEnd(state->builder, bb_end);
}

static void cg_stmt_while(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    LLVMValueRef func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(state->builder));

    LLVMBasicBlockRef bb_top = LLVMAppendBasicBlockInContext(state->context, func, "while.cond");
    LLVMBasicBlockRef bb_then = LLVMCreateBasicBlockInContext(state->context, "while.then");
    LLVMBasicBlockRef bb_end = LLVMCreateBasicBlockInContext(state->context, "while.end");

    bool create_end = node->stmt_while.condition != NULL;

    LLVMBuildBr(state->builder, bb_top);

    // Condition
    LLVMPositionBuilderAtEnd(state->builder, bb_top);
    if(node->stmt_while.condition != NULL) {
        codegen_value_container_t condition = cg_expr(state, scope, node->stmt_while.condition);
        if(!ir_type_eq(condition.type, ir_type_get_bool())) diag_error(node->stmt_while.condition->source_location, "condition is not a boolean");
        LLVMBuildCondBr(state->builder, condition.value, bb_then, bb_end);
    } else {
        LLVMBuildBr(state->builder, bb_then);
    }

    // Body
    LLVMAppendExistingBasicBlock(func, bb_then);
    LLVMPositionBuilderAtEnd(state->builder, bb_then);
    cg(state, scope, node->stmt_while.body);
    LLVMBuildBr(state->builder, bb_top);

    // End
    if(!create_end) return;
    LLVMAppendExistingBasicBlock(func, bb_end);
    LLVMPositionBuilderAtEnd(state->builder, bb_end);
}

static codegen_value_container_t cg_expr_literal_numeric(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    ir_type_t *type = ir_type_get_uint();
    return (codegen_value_container_t) {
        .type = type,
        .value = LLVMConstInt(llvm_type(state, type), node->expr_literal.numeric_value, type->integer.is_signed)
    };
}

static codegen_value_container_t cg_expr_literal_char(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    ir_type_t *type = ir_type_get_char();
    return (codegen_value_container_t) {
        .type = type,
        .value = LLVMConstInt(llvm_type(state, type), node->expr_literal.char_value, false)
    };
}

static codegen_value_container_t cg_expr_literal_string(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    size_t size = strlen(node->expr_literal.string_value) + 1;
    ir_type_t *type = ir_type_array_make(ir_type_get_char(), size);

    LLVMValueRef value = LLVMAddGlobal(state->module, llvm_type(state, type), "expr.literal_string");
    LLVMSetLinkage(value, LLVMInternalLinkage);
    LLVMSetGlobalConstant(value, 1);
    LLVMSetInitializer(value, LLVMConstStringInContext(state->context, node->expr_literal.string_value, size, true));

    return (codegen_value_container_t) {
        .type = type,
        .value = value
    };
}

static codegen_value_container_t cg_expr_literal_bool(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    ir_type_t *type = ir_type_get_bool();
    return (codegen_value_container_t) {
        .type = type,
        .value = LLVMConstInt(llvm_type(state, type), node->expr_literal.bool_value, false)
    };
}

static codegen_value_container_t cg_expr_binary(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    codegen_value_container_t right = cg_expr(state, scope, node->expr_binary.right);
    codegen_value_container_t left = cg_expr_ext(state, scope, node->expr_binary.left, false);

    ir_type_t *type = right.type;
    if(!ir_type_eq(type, left.type)) diag_error(node->source_location, "conflicting types in binary expression");

    if(node->expr_binary.operation == IR_NODE_BINARY_OPERATION_ASSIGN) {
        if(!left.iref) diag_error(node->source_location, "invalid lvalue");
        LLVMBuildStore(state->builder, right.value, left.value);
        return right;
    }

    left = resolve_iref(state, left);
    switch(node->expr_binary.operation) {
        case IR_NODE_BINARY_OPERATION_EQUAL: return (codegen_value_container_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, LLVMIntEQ, left.value, right.value, "expr.binary.eq")
        };
        case IR_NODE_BINARY_OPERATION_NOT_EQUAL: return (codegen_value_container_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, LLVMIntNE, left.value, right.value, "expr.binary.ne")
        };
        default: break;
    }

    if(type->kind != IR_TYPE_KIND_INTEGER) diag_error(node->source_location, "invalid type in binary expression");
    switch(node->expr_binary.operation) {
        case IR_NODE_BINARY_OPERATION_ADDITION: return (codegen_value_container_t) {
            .type = type,
            .value = LLVMBuildAdd(state->builder, left.value, right.value, "expr.binary.add")
        };
        case IR_NODE_BINARY_OPERATION_SUBTRACTION: return (codegen_value_container_t) {
            .type = type,
            .value = LLVMBuildSub(state->builder, left.value, right.value, "expr.binary.sub")
        };
        case IR_NODE_BINARY_OPERATION_MULTIPLICATION: return (codegen_value_container_t) {
            .type = type,
            .value = LLVMBuildMul(state->builder, left.value, right.value, "expr.binary.mul")
        };
        case IR_NODE_BINARY_OPERATION_DIVISION: return (codegen_value_container_t) {
            .type = type,
            .value = type->integer.is_signed ? LLVMBuildSDiv(state->builder, left.value, right.value, "expr.binary.sdiv") : LLVMBuildUDiv(state->builder, left.value, right.value, "expr.binary.udiv")
        };
        case IR_NODE_BINARY_OPERATION_MODULO: return (codegen_value_container_t) {
            .type = type,
            .value = type->integer.is_signed ? LLVMBuildSRem(state->builder, left.value, right.value, "expr.binary.srem") : LLVMBuildURem(state->builder, left.value, right.value, "expr.binary.urem")
        };
        case IR_NODE_BINARY_OPERATION_GREATER: return (codegen_value_container_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, type->integer.is_signed ? LLVMIntSGT : LLVMIntUGT, left.value, right.value, "expr.binary.gt")
        };
        case IR_NODE_BINARY_OPERATION_GREATER_EQUAL: return (codegen_value_container_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, type->integer.is_signed ? LLVMIntSGE : LLVMIntUGE, left.value, right.value, "expr.binary.ge")
        };
        case IR_NODE_BINARY_OPERATION_LESS: return (codegen_value_container_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, type->integer.is_signed ? LLVMIntSLT : LLVMIntULT, left.value, right.value, "expr.binary.lt")
        };
        case IR_NODE_BINARY_OPERATION_LESS_EQUAL: return (codegen_value_container_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, type->integer.is_signed ? LLVMIntSLE : LLVMIntULE, left.value, right.value, "expr.binary.le")
        };
        default: assert(false);
    }
}

static codegen_value_container_t cg_expr_unary(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    codegen_value_container_t operand = cg_expr_ext(state, scope, node->expr_unary.operand, false);
    if(node->expr_unary.operation == IR_NODE_UNARY_OPERATION_REF) {
        if(!operand.iref) diag_error(node->source_location, "references can only be made to variables");
        return (codegen_value_container_t) {
            .type = ir_type_pointer_make(operand.type),
            .value = operand.value
        };
    }

    operand = resolve_iref(state, operand);
    switch(node->expr_unary.operation) {
        case IR_NODE_UNARY_OPERATION_DEREF:
            if(operand.type->kind != IR_TYPE_KIND_POINTER) diag_error(node->source_location, "unary operation \"dereference\" on a non-pointer value");
            ir_type_t *type = operand.type->pointer.referred;
            return (codegen_value_container_t) {
                .iref = true,
                .type = type,
                .value = operand.value
            };
        case IR_NODE_UNARY_OPERATION_NOT:
            if(operand.type->kind != IR_TYPE_KIND_INTEGER) diag_error(node->source_location, "unary operation \"not\" on a non-numeric value");
            return (codegen_value_container_t) {
                .type = ir_type_get_bool(),
                .value = LLVMBuildICmp(state->builder, LLVMIntEQ, operand.value, LLVMConstInt(llvm_type(state, operand.type), 0, false), "")
            };
        case IR_NODE_UNARY_OPERATION_NEGATIVE:
            if(operand.type->kind != IR_TYPE_KIND_INTEGER) diag_error(node->source_location, "unary operation \"negative\" on a non-numeric value");
            return (codegen_value_container_t) {
                .type = operand.type,
                .value = LLVMBuildNeg(state->builder, operand.value, "")
            };
        default: assert(false);
    }
}

static codegen_value_container_t cg_expr_variable(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    codegen_variable_t *var = scope_get_variable(scope, node->expr_variable.name);
    if(var == NULL) diag_error(node->source_location, "referenced an undefined variable `%s`", node->expr_variable.name);
    return (codegen_value_container_t) {
        .iref = true,
        .type = var->type,
        .value = var->ref
    };
}

static codegen_value_container_t cg_expr_call(codegen_state_t *state, codegen_scope_t *scope, symbol_t *selected_module, ir_node_t *node) {
    symbol_t *symbol;
    if(selected_module != NULL) {
        symbol = symbol_list_find_type(&selected_module->module.symbols, node->expr_call.function_name, SYMBOL_TYPE_FUNCTION);
    } else {
        symbol = state_get_function(state, node->expr_call.function_name, true);
    }
    if(symbol == NULL) diag_error(node->source_location, "call to an unknown function `%s`", node->expr_call.function_name);

    if(node->expr_call.arguments.count < symbol->function.prototype->argument_count) diag_error(node->source_location, "missing arguments");
    if(!symbol->function.prototype->varargs && node->expr_call.arguments.count > symbol->function.prototype->argument_count) diag_error(node->source_location, "invalid number of arguments");

    size_t argument_count = ir_node_list_count(&node->expr_call.arguments);
    LLVMValueRef arguments[node->expr_call.arguments.count];
    IR_NODE_LIST_FOREACH(&node->expr_call.arguments, {
        codegen_value_container_t argument = cg_expr(state, scope, node);
        if(symbol->function.prototype->argument_count > i && !ir_type_eq(argument.type, symbol->function.prototype->arguments[i].type)) diag_error(node->source_location, "argument has invalid type");
        arguments[i] = argument.value;
    });

    return (codegen_value_container_t) {
        .type = symbol->function.prototype->return_type,
        .value = LLVMBuildCall2(state->builder, symbol->function.type, symbol->function.value, arguments, argument_count, "")
    };
}

static codegen_value_container_t cg_expr_selector(codegen_state_t *state, codegen_scope_t *scope, symbol_t *selected_module, ir_node_t *node) {
    selected_module = symbol_list_find_type(selected_module == NULL ? &state->symbols : &selected_module->module.symbols, node->expr_selector.name, SYMBOL_TYPE_MODULE);
    if(selected_module == NULL) diag_error(node->source_location, "unknown module `%s`", node->expr_selector.name);
    return cg_expr_ext_selected_module(state, scope, selected_module, node->expr_selector.value);
}

static codegen_value_container_t cg_expr_tuple(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    LLVMValueRef values[node->expr_tuple.values.count];
    ir_type_t **types = malloc(node->expr_tuple.values.count * sizeof(ir_type_t *));
    IR_NODE_LIST_FOREACH(&node->expr_tuple.values, {
        codegen_value_container_t value = cg_expr(state, scope, node);
        types[i] = value.type;
        values[i] = value.value;
    });

    ir_type_t *type = ir_type_tuple_make(node->expr_tuple.values.count, types);
    LLVMTypeRef type_llvm = llvm_type(state, type);

    LLVMValueRef allocated_tuple = LLVMBuildAlloca(state->builder, type_llvm, "expr.tuple");
    for(size_t i = 0; i < node->expr_tuple.values.count; i++) {
        LLVMValueRef member_ptr = LLVMBuildStructGEP2(state->builder, type_llvm, allocated_tuple, i, "");
        LLVMBuildStore(state->builder, values[i], member_ptr);
    }

    return (codegen_value_container_t) {
        .iref = true,
        .type = type,
        .value = allocated_tuple
    };
}

static codegen_value_container_t cg_expr_cast(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    codegen_value_container_t value_from = cg_expr(state, scope, node->expr_cast.value);
    if(ir_type_eq(node->expr_cast.type, value_from.type)) return value_from;

    ir_type_t *type_to = node->expr_cast.type;
    ir_type_t *type_from = value_from.type;

    if(type_to->kind == IR_TYPE_KIND_INTEGER && value_from.type->kind == IR_TYPE_KIND_INTEGER) {
        LLVMValueRef value_to;
        if(type_from->integer.bit_size == type_to->integer.bit_size) {
            value_to = value_from.value;
        } else {
            if(type_from->integer.bit_size > type_to->integer.bit_size) {
                value_to = LLVMBuildTrunc(state->builder, value_from.value, llvm_type(state, type_to), "cast.trunc");
            } else {
                if(type_to->integer.is_signed) {
                    value_to = LLVMBuildSExt(state->builder, value_from.value, llvm_type(state, type_to), "cast.sext");
                } else {
                    value_to = LLVMBuildZExt(state->builder, value_from.value, llvm_type(state, type_to), "cast.zext");
                }
            }
        }
        return (codegen_value_container_t) { .type = type_to, .value = value_to };
    }

    if(type_from->kind == IR_TYPE_KIND_POINTER && type_to->kind == IR_TYPE_KIND_POINTER) return (codegen_value_container_t) {
        .type = type_to,
        .value = value_from.value
    };

    if(type_from->kind == IR_TYPE_KIND_POINTER && type_to->kind == IR_TYPE_KIND_INTEGER) return (codegen_value_container_t) {
        .type = type_to,
        .value = LLVMBuildPtrToInt(state->builder, value_from.value, llvm_type(state, type_to), "cast.ptrtoint")
    };

    if(type_from->kind == IR_TYPE_KIND_INTEGER && type_to->kind == IR_TYPE_KIND_POINTER) return (codegen_value_container_t) {
        .type = type_to,
        .value = LLVMBuildIntToPtr(state->builder, value_from.value, llvm_type(state, type_to), "cast.inttoptr")
    };

    if(type_from->kind == IR_TYPE_KIND_ARRAY && type_to->kind == IR_TYPE_KIND_POINTER) {
        if(ir_type_eq(type_from->array.type, type_to->pointer.referred)) return (codegen_value_container_t) {
            .type = type_to,
            .value = value_from.value // TODO: validate we dont actually need a cast here
        };
    }

    diag_error(node->source_location, "invalid cast");
}

static codegen_value_container_t cg_expr_access_index(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    codegen_value_container_t value = cg_expr_ext(state, scope, node->expr_access_index.value, false);
    codegen_value_container_t index = cg_expr(state, scope, node->expr_access_index.index);

    if(index.type->kind != IR_TYPE_KIND_INTEGER) diag_error(node->source_location, "invalid type for indexing");
    if(value.type->kind != IR_TYPE_KIND_ARRAY) diag_error(node->source_location, "can only index arrays");

    return (codegen_value_container_t) {
        .iref = true,
        .type = value.type->array.type,
        .value = LLVMBuildGEP2(state->builder, llvm_type(state, value.type), value.value, (LLVMValueRef[]) { LLVMConstInt(LLVMInt64TypeInContext(state->context), 0, false), index.value }, 2, "expr.access_index")
    };
}

static codegen_value_container_t cg_expr_access_index_const(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    codegen_value_container_t value = cg_expr_ext(state, scope, node->expr_access_index_const.value, false);
    if(!value.iref) diag_error(node->source_location, "invalid type for constant indexing");

    ir_type_t *member_type;
    switch(value.type->kind) {
        case IR_TYPE_KIND_TUPLE:
            if(node->expr_access_index_const.index >= value.type->tuple.type_count) diag_error(node->source_location, "index out of bounds");
            member_type = value.type->tuple.types[node->expr_access_index_const.index];
            break;
        case IR_TYPE_KIND_ARRAY:
            if(node->expr_access_index_const.index >= value.type->array.size) diag_error(node->source_location, "index out of bounds");
            member_type = value.type->array.type;
            break;
        default: diag_error(node->source_location, "invalid type for constant indexing");
    }

    LLVMValueRef member_ptr = LLVMBuildStructGEP2(state->builder, llvm_type(state, value.type), value.value, node->expr_access_index_const.index, "expr.access_index_const");
    return (codegen_value_container_t) {
        .iref = true,
        .type = member_type,
        .value = member_ptr
    };
}

static codegen_value_container_t cg_expr_ext_selected_module(codegen_state_t *state, codegen_scope_t *scope, symbol_t *selected_module, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_EXPR_CALL: return cg_expr_call(state, scope, selected_module, node);
        case IR_NODE_TYPE_EXPR_SELECTOR: return cg_expr_selector(state, scope, selected_module, node);
        default: assert(false);
    }
    __builtin_unreachable();
}

static codegen_value_container_t cg_expr_ext(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node, bool do_resolve_iref) {
    codegen_value_container_t value;
    switch(node->type) {
        case IR_NODE_TYPE_ROOT:

        case IR_NODE_TYPE_TLC_MODULE:
        case IR_NODE_TYPE_TLC_FUNCTION:
        case IR_NODE_TYPE_TLC_EXTERN:

        case IR_NODE_TYPE_STMT_NOOP:
        case IR_NODE_TYPE_STMT_BLOCK:
        case IR_NODE_TYPE_STMT_DECLARATION:
        case IR_NODE_TYPE_STMT_EXPRESSION:
        case IR_NODE_TYPE_STMT_RETURN:
        case IR_NODE_TYPE_STMT_IF:
        case IR_NODE_TYPE_STMT_WHILE:
            assert(false);

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: value = cg_expr_literal_numeric(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: value = cg_expr_literal_string(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: value = cg_expr_literal_char(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: value = cg_expr_literal_bool(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_BINARY: value = cg_expr_binary(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_UNARY: value = cg_expr_unary(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_VARIABLE: value = cg_expr_variable(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_CALL: value = cg_expr_call(state, scope, NULL, node); break;
        case IR_NODE_TYPE_EXPR_TUPLE: value = cg_expr_tuple(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_CAST: value = cg_expr_cast(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX: value = cg_expr_access_index(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX_CONST: value = cg_expr_access_index_const(state, scope, node); break;
        case IR_NODE_TYPE_EXPR_SELECTOR: value = cg_expr_selector(state, scope, NULL, node); break;
    }
    if(do_resolve_iref) return resolve_iref(state, value);
    return value;
}

static codegen_value_container_t cg_expr(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    return cg_expr_ext(state, scope, node, true);
}

static void cg(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_ROOT: assert(false);

        case IR_NODE_TYPE_TLC_MODULE: cg_tlc_module(state, scope, node); break;
        case IR_NODE_TYPE_TLC_FUNCTION: cg_tlc_function(state, scope, node); break;
        case IR_NODE_TYPE_TLC_EXTERN: cg_tlc_extern(state, scope, node); break;

        case IR_NODE_TYPE_STMT_NOOP: break;
        case IR_NODE_TYPE_STMT_BLOCK: cg_stmt_block(state, scope, node); break;
        case IR_NODE_TYPE_STMT_DECLARATION: cg_stmt_declaration(state, scope, node); break;
        case IR_NODE_TYPE_STMT_EXPRESSION: cg_expr(state, scope, node->stmt_expression.expression); break;
        case IR_NODE_TYPE_STMT_RETURN: cg_stmt_return(state, scope, node); break;
        case IR_NODE_TYPE_STMT_IF: cg_stmt_if(state, scope, node); break;
        case IR_NODE_TYPE_STMT_WHILE: cg_stmt_while(state, scope, node); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC:
        case IR_NODE_TYPE_EXPR_LITERAL_STRING:
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR:
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL:
        case IR_NODE_TYPE_EXPR_BINARY:
        case IR_NODE_TYPE_EXPR_UNARY:
        case IR_NODE_TYPE_EXPR_VARIABLE:
        case IR_NODE_TYPE_EXPR_TUPLE:
        case IR_NODE_TYPE_EXPR_CALL:
        case IR_NODE_TYPE_EXPR_CAST:
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX:
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX_CONST:
        case IR_NODE_TYPE_EXPR_SELECTOR:
            assert(false);
    }
}

static void cg_root(ir_node_t *node, LLVMContextRef context, LLVMModuleRef module) {
    assert(node->type == IR_NODE_TYPE_ROOT);

    codegen_state_t state = {};
    state.context = context;
    state.module = module;
    state.builder = LLVMCreateBuilderInContext(state.context);
    state.symbols = SYMBOL_LIST_INIT;
    state.current_module = NULL;

    cg_list(&state, NULL, &node->root.tlc_nodes);

    symbol_list_free(state.symbols);
    LLVMDisposeBuilder(state.builder);
}

void codegen(ir_node_t *node, const char *path, const char *passes) {
    assert(node->type == IR_NODE_TYPE_ROOT);

    char *error_message;

    // OPTIMIZE: this is kind of lazy
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllTargets();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    const char *triple = LLVMGetDefaultTargetTriple();

    LLVMTargetRef target;
    if(LLVMGetTargetFromTriple(triple, &target, &error_message) != 0) log_fatal("failed to create target (%s)", error_message);

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelDefault);
    if(machine == NULL) log_fatal("failed to create target machine");

    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("CharonModule", context);

    cg_root(node, context, module);
    error_message = NULL;
    LLVMVerifyModule(module, LLVMReturnStatusAction, &error_message);
    if(error_message != NULL) {
        if(strlen(error_message) != 0) log_fatal("failed to verify module (%s)", error_message);
        LLVMDisposeMessage(error_message);
    }

    if(passes != NULL) {
        LLVMErrorRef error = LLVMRunPasses(module, passes, NULL, LLVMCreatePassBuilderOptions());
        if(error != NULL) log_warning("llvm ir optimization failed (%s)", LLVMGetErrorMessage(error));
    }

    LLVMSetTarget(module, triple);
    char *layout = LLVMCopyStringRepOfTargetData(LLVMCreateTargetDataLayout(machine));
    LLVMSetDataLayout(module, layout);
    LLVMDisposeMessage(layout);

    if(LLVMTargetMachineEmitToFile(machine, module, path, LLVMObjectFile, &error_message) != 0) log_fatal("emit failed (%s)", error_message);

    LLVMDisposeModule(module);
    LLVMContextDispose(context);
}

void codegen_ir(ir_node_t *node, const char *path) {
    assert(node->type == IR_NODE_TYPE_ROOT);

    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("CharonModule", context);

    cg_root(node, context, module);

    char *error_message;
    LLVMBool failure = LLVMPrintModuleToFile(module, path, &error_message);
    if(failure) log_fatal("emit failed (%s)\n", error_message);

    LLVMDisposeModule(module);
    LLVMContextDispose(context);
}