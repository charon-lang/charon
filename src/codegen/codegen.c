#include "codegen.h"

#include "lib/diag.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    LLVMModuleRef module;
    LLVMContextRef context;
    LLVMBuilderRef builder;
} codegen_state_t;

typedef struct {
    ir_type_t *type;
    LLVMValueRef value;
} codegen_value_t;

typedef struct {
    const char *name;
    ir_type_t *type;
    LLVMValueRef ref;
} codegen_variable_t;

typedef struct codegen_scope {
    codegen_variable_t *variables;
    size_t variable_count;

    struct codegen_scope *parent;
} codegen_scope_t;

static LLVMTypeRef llvm_type(codegen_state_t *state, ir_type_t *type) {
    if(type->kind == IR_TYPE_KIND_INTEGER) {
        switch(type->integer.bit_size) {
            case 1: return LLVMInt1TypeInContext(state->context);
            case 8: return LLVMInt8TypeInContext(state->context);
            case 16: return LLVMInt16TypeInContext(state->context);
            case 32: return LLVMInt32TypeInContext(state->context);
            case 64: return LLVMInt64TypeInContext(state->context);
        }
    }
    assert(false);
}

static void scope_add_variable(codegen_scope_t *scope, const char *name, ir_type_t *type, LLVMValueRef ref) {
    scope->variables = realloc(scope->variables, ++scope->variable_count * sizeof(codegen_variable_t));
    scope->variables[scope->variable_count - 1] = (codegen_variable_t) { .name = name, .type = type, .ref = ref };
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

static void cg(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node);
static codegen_value_t cg_expr(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node);

static void cg_list(codegen_state_t *state, codegen_scope_t *scope, ir_node_list_t *list) {
    ir_node_t *node = list->first;
    while(node != NULL) {
        cg(state, scope, node);
        node = node->next;
    }
}

static void cg_tlc_function(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    LLVMTypeRef type_fn = LLVMFunctionType(LLVMVoidTypeInContext(state->context), NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(state->module, node->tlc_function.name, type_fn);
    LLVMBasicBlockRef bb_entry = LLVMAppendBasicBlockInContext(state->context, fn, node->tlc_function.name);
    LLVMPositionBuilderAtEnd(state->builder, bb_entry);

    scope = scope_enter(scope);
    cg_list(state, scope, &node->tlc_function.statements);
    scope = scope_exit(scope);

    LLVMBuildRetVoid(state->builder); // TODO
}

static void cg_stmt_block(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    scope = scope_enter(scope);
    cg_list(state, scope, &node->stmt_block.statements);
    scope = scope_exit(scope);
}

static void cg_stmt_declaration(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    LLVMValueRef func_parent = LLVMGetBasicBlockParent(LLVMGetInsertBlock(state->builder));

    LLVMValueRef value = NULL;
    ir_type_t *type = node->stmt_declaration.type;
    if(node->stmt_declaration.initial != NULL) {
        // TODO: if type is explicitly defined, ENFORCE!!
        codegen_value_t cg_value = cg_expr(state, scope, node->stmt_declaration.initial);
        value = cg_value.value;
        if(type == NULL) type = cg_value.type;
    }
    if(type == NULL) diag_error(node->source_location, "declaration is missing both an explicit and inferred type");

    LLVMBasicBlockRef bb_entry = LLVMGetEntryBasicBlock(func_parent);
    LLVMBuilderRef entry_builder = LLVMCreateBuilderInContext(state->context);
    LLVMPositionBuilder(entry_builder, bb_entry, LLVMGetFirstInstruction(bb_entry));
    LLVMValueRef ref = LLVMBuildAlloca(entry_builder, llvm_type(state, node->stmt_declaration.type), node->stmt_declaration.name);
    LLVMDisposeBuilder(entry_builder);
    if(value != NULL) LLVMBuildStore(state->builder, value, ref);

    scope_add_variable(scope, node->stmt_declaration.name, type, ref);
}

static codegen_value_t cg_expr_literal_numeric(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    ir_type_t *type = ir_type_get_uint();
    return (codegen_value_t) {
        .type = type,
        .value = LLVMConstInt(llvm_type(state, type), node->expr_literal.numeric_value, false) // TODO: maybe sign extend sometime?
    };
}

static codegen_value_t cg_expr_literal_char(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    ir_type_t *type = ir_type_get_char();
    return (codegen_value_t) {
        .type = type,
        .value = LLVMConstInt(llvm_type(state, type), node->expr_literal.char_value, false)
    };
}

static codegen_value_t cg_expr_literal_bool(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    ir_type_t *type = ir_type_get_bool();
    return (codegen_value_t) {
        .type = type,
        .value = LLVMConstInt(llvm_type(state, type), node->expr_literal.bool_value, false)
    };
}

static codegen_value_t cg_expr_binary(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    codegen_value_t right = cg_expr(state, scope, node->expr_binary.right);
    codegen_value_t left = cg_expr(state, scope, node->expr_binary.left);
    if(!ir_type_eq(right.type, left.type)) diag_error(node->source_location, "conflicting types in binary expression");

    ir_type_t *type = right.type;

    switch(node->expr_binary.operation) {
        case IR_NODE_BINARY_OPERATION_EQUAL: return (codegen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, LLVMIntEQ, left.value, right.value, "expr.binary.eq")
        };
        case IR_NODE_BINARY_OPERATION_NOT_EQUAL: return (codegen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, LLVMIntNE, left.value, right.value, "expr.binary.ne")
        };
        default: break;
    }

    if(type->kind != IR_TYPE_KIND_INTEGER) diag_error(node->source_location, "invalid type in binary expression");

    bool is_signed = type->integer.is_signed;
    switch(node->expr_binary.operation) {
        case IR_NODE_BINARY_OPERATION_ADDITION: return (codegen_value_t) {
            .type = type,
            .value = LLVMBuildAdd(state->builder, left.value, right.value, "expr.binary.add")
        };
        case IR_NODE_BINARY_OPERATION_SUBTRACTION: return (codegen_value_t) {
            .type = type,
            .value = LLVMBuildSub(state->builder, left.value, right.value, "expr.binary.sub")
        };
        case IR_NODE_BINARY_OPERATION_MULTIPLICATION: return (codegen_value_t) {
            .type = type,
            .value = LLVMBuildMul(state->builder, left.value, right.value, "expr.binary.mul")
        };
        case IR_NODE_BINARY_OPERATION_DIVISION: return (codegen_value_t) {
            .type = type,
            .value = is_signed ? LLVMBuildSDiv(state->builder, left.value, right.value, "expr.binary.sdiv") : LLVMBuildUDiv(state->builder, left.value, right.value, "expr.binary.udiv")
        };
        case IR_NODE_BINARY_OPERATION_MODULO: return (codegen_value_t) {
            .type = type,
            .value = is_signed ? LLVMBuildSRem(state->builder, left.value, right.value, "expr.binary.srem") : LLVMBuildURem(state->builder, left.value, right.value, "expr.binary.urem")
        };
        case IR_NODE_BINARY_OPERATION_GREATER: return (codegen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, is_signed ? LLVMIntSGT : LLVMIntUGT, left.value, right.value, "expr.binary.gt")
        };
        case IR_NODE_BINARY_OPERATION_GREATER_EQUAL: return (codegen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, is_signed ? LLVMIntSGE : LLVMIntUGE, left.value, right.value, "expr.binary.ge")
        };
        case IR_NODE_BINARY_OPERATION_LESS: return (codegen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, is_signed ? LLVMIntSLT : LLVMIntULT, left.value, right.value, "expr.binary.lt")
        };
        case IR_NODE_BINARY_OPERATION_LESS_EQUAL: return (codegen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(state->builder, is_signed ? LLVMIntSLE : LLVMIntULE, left.value, right.value, "expr.binary.le")
        };
        default: assert(false);
    }
}

static codegen_value_t cg_expr_variable(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    codegen_variable_t *var = scope_get_variable(scope, node->expr_variable.name);
    if(var == NULL) diag_error(node->source_location, "referenced an undefined variable `%s`", node->expr_variable.name);
    return (codegen_value_t) {
        .type = var->type,
        .value = LLVMBuildLoad2(state->builder, llvm_type(state, var->type), var->ref, "expr.variable.load")
    };
}

static codegen_value_t cg_expr(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_ROOT:

        case IR_NODE_TYPE_TLC_FUNCTION:

        case IR_NODE_TYPE_STMT_BLOCK:
        case IR_NODE_TYPE_STMT_DECLARATION:
        case IR_NODE_TYPE_STMT_EXPRESSION:
            assert(false);

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: return cg_expr_literal_numeric(state, scope, node);
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: assert(false);
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: return cg_expr_literal_char(state, scope, node);
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: return cg_expr_literal_bool(state, scope, node);
        case IR_NODE_TYPE_EXPR_BINARY: return cg_expr_binary(state, scope, node);
        case IR_NODE_TYPE_EXPR_UNARY: assert(false);
        case IR_NODE_TYPE_EXPR_VARIABLE: return cg_expr_variable(state, scope, node);
    }
    assert(false);
}

static void cg(codegen_state_t *state, codegen_scope_t *scope, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_ROOT: cg_list(state, scope, &node->root.tlc_nodes); break;

        case IR_NODE_TYPE_TLC_FUNCTION: cg_tlc_function(state, scope, node); break;

        case IR_NODE_TYPE_STMT_BLOCK: cg_stmt_block(state, scope, node); break;
        case IR_NODE_TYPE_STMT_DECLARATION: cg_stmt_declaration(state, scope, node); break;
        case IR_NODE_TYPE_STMT_EXPRESSION: cg_expr(state, scope, node->stmt_expression.expression); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC:
        case IR_NODE_TYPE_EXPR_LITERAL_STRING:
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR:
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL:
        case IR_NODE_TYPE_EXPR_BINARY:
        case IR_NODE_TYPE_EXPR_UNARY:
        case IR_NODE_TYPE_EXPR_VARIABLE:
            assert(false);
    }
}

void codegen(ir_node_t *node, const char *dest, const char *passes) {
    codegen_state_t state = {};
    state.context = LLVMContextCreate();
    state.module = LLVMModuleCreateWithNameInContext("CharonModule", state.context);
    state.builder = LLVMCreateBuilderInContext(state.context);
    cg(&state, NULL, node);

    LLVMRunPasses(state.module, passes, NULL, LLVMCreatePassBuilderOptions());
    LLVMPrintModuleToFile(state.module, dest, NULL);

    LLVMDisposeBuilder(state.builder);
    LLVMDisposeModule(state.module);
    LLVMContextDispose(state.context);
}