#include "codegen.h"

#include "lib/diag.h"
#include "lib/log.h"
#include "ir/node.h"
#include "codegen/symbol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CASE_TLC(BODY) \
        case IR_NODE_TYPE_TLC_MODULE: \
        case IR_NODE_TYPE_TLC_FUNCTION: \
        case IR_NODE_TYPE_TLC_EXTERN: \
            BODY; break;

#define CASE_STMT(BODY) \
        case IR_NODE_TYPE_STMT_NOOP: \
        case IR_NODE_TYPE_STMT_BLOCK: \
        case IR_NODE_TYPE_STMT_DECLARATION: \
        case IR_NODE_TYPE_STMT_EXPRESSION: \
        case IR_NODE_TYPE_STMT_RETURN: \
        case IR_NODE_TYPE_STMT_IF: \
        case IR_NODE_TYPE_STMT_WHILE: \
            BODY; break;

#define CASE_EXPRESSION(BODY) \
        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: \
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: \
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: \
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: \
        case IR_NODE_TYPE_EXPR_BINARY: \
        case IR_NODE_TYPE_EXPR_UNARY: \
        case IR_NODE_TYPE_EXPR_VARIABLE: \
        case IR_NODE_TYPE_EXPR_TUPLE: \
        case IR_NODE_TYPE_EXPR_CALL: \
        case IR_NODE_TYPE_EXPR_CAST: \
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX: \
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX_CONST: \
        case IR_NODE_TYPE_EXPR_SELECTOR: \
            BODY; break;

typedef struct {
    LLVMContextRef llvm_context;
    LLVMBuilderRef llvm_builder;
    LLVMModuleRef llvm_module;

    symbol_table_t root_symtab;

    struct {
        ir_type_t *type;
        bool has_returned;
    } return_state;
} context_t;

typedef struct scope scope_t;

typedef struct {
    scope_t *scope;
    const char *name;
    ir_type_t *type;
    LLVMValueRef llvm_value;
} scope_variable_t;

struct scope {
    scope_t *parent;
    scope_variable_t *variables;
    size_t variable_count;
};

typedef struct {
    bool is_ref;
    ir_type_t *type;
    LLVMValueRef llvm_value;
} value_t;

static char *mangle_name(const char *name, symbol_t *parent) {
    char *mangled_name = strdup(name);
    while(parent != NULL) {
        assert(parent->kind == SYMBOL_KIND_MODULE);

        size_t name_length = strlen(mangled_name);
        size_t module_name_length= strlen(parent->name);
        char *new = malloc(module_name_length + 2 + name_length + 1);
        memcpy(new, parent->name, module_name_length);
        memset(&new[module_name_length], ':', 2);
        memcpy(&new[module_name_length + 2], mangled_name, name_length);
        new[module_name_length + 2 + name_length] = '\0';

        free(mangled_name);
        mangled_name = new;
        parent = parent->table->parent;
    }
    return mangled_name;
}

static LLVMTypeRef ir_type_to_llvm(context_t *context, ir_type_t *type) {
    assert(type != NULL);
    switch(type->kind) {
        case IR_TYPE_KIND_VOID: return LLVMVoidTypeInContext(context->llvm_context);
        case IR_TYPE_KIND_INTEGER:
            switch(type->integer.bit_size) {
                case 1: return LLVMInt1TypeInContext(context->llvm_context);
                case 8: return LLVMInt8TypeInContext(context->llvm_context);
                case 16: return LLVMInt16TypeInContext(context->llvm_context);
                case 32: return LLVMInt32TypeInContext(context->llvm_context);
                case 64: return LLVMInt64TypeInContext(context->llvm_context);
                default: assert(false);
            }
        case IR_TYPE_KIND_POINTER: return LLVMPointerTypeInContext(context->llvm_context, 0);
        case IR_TYPE_KIND_ARRAY: return LLVMArrayType2(ir_type_to_llvm(context, type->array.type), type->array.size);
        case IR_TYPE_KIND_TUPLE: {
            LLVMTypeRef types[type->tuple.type_count];
            for(size_t i = 0; i < type->tuple.type_count; i++) types[i] = ir_type_to_llvm(context, type->tuple.types[i]);
            return LLVMStructTypeInContext(context->llvm_context, types, type->tuple.type_count, false);
        }
        case IR_TYPE_KIND_FUNCTION: {
            LLVMTypeRef fn_arguments[type->function.argument_count];
            for(size_t i = 0; i < type->function.argument_count; i++) fn_arguments[i] = ir_type_to_llvm(context, type->function.arguments[i]);
            LLVMTypeRef fn_return_type = LLVMVoidTypeInContext(context->llvm_context);
            fn_return_type = ir_type_to_llvm(context, type->function.return_type);
            return LLVMFunctionType(fn_return_type, fn_arguments, type->function.argument_count, type->function.varargs);
        }
    }
    assert(false);
}

static scope_t *scope_enter(scope_t *current) {
    scope_t *scope = malloc(sizeof(scope_t));
    scope->parent = current;
    scope->variable_count = 0;
    scope->variables = NULL;
    return scope;
}

static scope_t *scope_exit(scope_t *current) {
    scope_t *parent = current->parent;
    free(current->variables);
    free(current);
    return parent;
}

static void scope_variable_add(scope_t *scope, const char *name, ir_type_t *type, LLVMValueRef llvm_value) {
    scope->variables = reallocarray(scope->variables, ++scope->variable_count, sizeof(scope_variable_t));
    scope->variables[scope->variable_count - 1] = (scope_variable_t) { .scope = scope, .name = name, .type = type, .llvm_value = llvm_value };
}

static scope_variable_t *scope_variable_find(scope_t *scope, const char *name) {
    assert(scope != NULL);
    for(size_t i = 0; i < scope->variable_count; i++) {
        if(strcmp(name, scope->variables[i].name) != 0) continue;
        return &scope->variables[i];
    }
    if(scope->parent == NULL) return NULL;
    return scope_variable_find(scope->parent, name);
}

static value_t resolve_ref(context_t *context, value_t value) {
    if(!value.is_ref) return value;
    return (value_t) {
        .type = value.type,
        .llvm_value = LLVMBuildLoad2(context->llvm_builder, ir_type_to_llvm(context, value.type), value.llvm_value, "resolve_ref")
    };
}

// //
// Generate symbol table
//

static void gst(context_t *context, symbol_table_t *symtab, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_ROOT:
            IR_NODE_LIST_FOREACH(&node->root.tlcs, gst(context, symtab, node));
            break;
        case IR_NODE_TYPE_TLC_MODULE:
            if(symbol_table_exists(symtab, node->tlc_module.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_module.name);
            symbol_t *symbol = symbol_table_add_module(symtab, node->tlc_module.name);
            IR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, gst(context, &symbol->module.symtab, node));
            break;
        case IR_NODE_TYPE_TLC_FUNCTION: {
            if(symbol_table_exists(symtab, node->tlc_function.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_function.name);
            LLVMValueRef llvm_fn = LLVMAddFunction(context->llvm_module, mangle_name(node->tlc_function.name, symtab->parent), ir_type_to_llvm(context, node->tlc_function.type));
            symbol_table_add_function(symtab, node->tlc_function.name, node->tlc_function.type, llvm_fn);
        } break;
        case IR_NODE_TYPE_TLC_EXTERN:
            if(symbol_table_exists(symtab, node->tlc_extern.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_extern.name);
            LLVMValueRef llvm_fn = LLVMAddFunction(context->llvm_module, mangle_name(node->tlc_extern.name, symtab->parent), ir_type_to_llvm(context, node->tlc_extern.type));
            symbol_table_add_function(symtab, node->tlc_extern.name, node->tlc_extern.type, llvm_fn);
            break;
        CASE_STMT(assert(false))
        CASE_EXPRESSION(assert(false))
    }
}

// //
// Codegen
//

#define CG_TLC_PARAMS context_t *context, symbol_table_t *symtab, ir_node_t *node
#define CG_STMT_PARAMS context_t *context, symbol_table_t *symtab, scope_t *scope, ir_node_t *node
#define CG_EXPR_PARAMS context_t *context, symbol_table_t *symtab, scope_t *scope, ir_node_t *node

static void cg_tlc(CG_TLC_PARAMS);
static void cg_stmt(CG_STMT_PARAMS);
static value_t cg_expr(CG_EXPR_PARAMS);
static value_t cg_expr_ext(CG_EXPR_PARAMS, bool do_resolve_ref);

// TLCs

static void cg_tlc_module(CG_TLC_PARAMS) {
    symbol_t *symbol = symbol_table_find_kind(symtab, node->tlc_function.name, SYMBOL_KIND_MODULE);
    assert(symbol != NULL);

    IR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, cg_tlc(context, &symbol->module.symtab, node));
}

static void cg_tlc_function(CG_TLC_PARAMS) {
    symbol_t *symbol = symbol_table_find_kind(symtab, node->tlc_function.name, SYMBOL_KIND_FUNCTION);
    assert(symbol != NULL);

    context->return_state.has_returned = false;
    context->return_state.type = symbol->function.type->function.return_type;

    scope_t *scope = scope_enter(NULL);

    LLVMBasicBlockRef bb_entry = LLVMAppendBasicBlockInContext(context->llvm_context, symbol->function.llvm_value, "tlc.function");
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_entry);

    for(size_t i = 0; i < symbol->function.type->function.argument_count; i++) {
        ir_type_t *param_type = symbol->function.type->function.arguments[i];
        const char *param_name = node->tlc_function.argument_names[i];

        LLVMValueRef param = LLVMBuildAlloca(context->llvm_builder, ir_type_to_llvm(context, param_type), param_name);
        LLVMBuildStore(context->llvm_builder, LLVMGetParam(symbol->function.llvm_value, i), param);
        scope_variable_add(scope, param_name, param_type, param);
    }

    cg_stmt(context, symtab, scope, node->tlc_function.statement);

    if(!context->return_state.has_returned) {
        if(context->return_state.type->kind != IR_TYPE_KIND_VOID) diag_error(node->source_location, "missing return");
        LLVMBuildRetVoid(context->llvm_builder);
    }

    scope_exit(scope);
}

// Statements

static void cg_stmt_block(CG_STMT_PARAMS) {
    scope = scope_enter(scope);
    IR_NODE_LIST_FOREACH(&node->stmt_block.statements, {
        cg_stmt(context, symtab, scope, node);
        if(context->return_state.has_returned) break;
    });
    scope = scope_exit(scope);
}

static void cg_stmt_declaration(CG_STMT_PARAMS) {
    scope_variable_t *var = scope_variable_find(scope, node->stmt_declaration.name);
    if(var != NULL) {
        if(var->scope == scope) diag_error(node->source_location, "redeclaration of `%s`", node->stmt_declaration.name);
        diag_warn(node->source_location, "declaration shadows `%s`", node->stmt_declaration.name);
    }

    LLVMValueRef func_parent = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));

    LLVMValueRef llvm_value = NULL;
    ir_type_t *type = node->stmt_declaration.type;
    if(node->stmt_declaration.initial != NULL) {
        value_t value = cg_expr(context, symtab, scope, node->stmt_declaration.initial);
        if(type == NULL) {
            type = value.type;
        } else {
            if(!ir_type_eq(value.type, type)) diag_error(node->source_location, "declarations initial value does not match its explicit type");
        }
        llvm_value = value.llvm_value;
    }
    if(type == NULL) diag_error(node->source_location, "declaration is missing both an explicit and inferred type");

    LLVMBasicBlockRef bb_entry = LLVMGetEntryBasicBlock(func_parent);
    LLVMBuilderRef entry_builder = LLVMCreateBuilderInContext(context->llvm_context);
    LLVMPositionBuilder(entry_builder, bb_entry, LLVMGetFirstInstruction(bb_entry));
    LLVMValueRef ref = LLVMBuildAlloca(entry_builder, ir_type_to_llvm(context, type), node->stmt_declaration.name);
    LLVMDisposeBuilder(entry_builder);
    if(llvm_value != NULL) LLVMBuildStore(context->llvm_builder, llvm_value, ref);

    scope_variable_add(scope, node->stmt_declaration.name, type, ref);
}

static void cg_stmt_return(CG_STMT_PARAMS) {
    context->return_state.has_returned = true;
    if(context->return_state.type->kind == IR_TYPE_KIND_VOID) {
        if(node->stmt_return.value) {
            value_t value = cg_expr(context, symtab, scope, node->stmt_return.value);
            if(value.type->kind != IR_TYPE_KIND_VOID) diag_error(node->source_location, "value returned from a function without a return type");
        }
        LLVMBuildRetVoid(context->llvm_builder);
        return;
    }
    if(node->stmt_return.value == NULL) diag_error(node->source_location, "no return value");
    value_t value = cg_expr(context, symtab, scope, node->stmt_return.value);
    if(!ir_type_eq(value.type, context->return_state.type)) diag_error(node->source_location, "invalid type");
    LLVMBuildRet(context->llvm_builder, value.llvm_value);
}

static void cg_stmt_if(CG_STMT_PARAMS) {
    LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));
    LLVMBasicBlockRef bb_then = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "if.then");
    LLVMBasicBlockRef bb_else = LLVMCreateBasicBlockInContext(context->llvm_context, "if.else");
    LLVMBasicBlockRef bb_end = LLVMCreateBasicBlockInContext(context->llvm_context, "if.end");

    value_t condition = cg_expr(context, symtab, scope, node->stmt_if.condition);
    if(!ir_type_eq(condition.type, ir_type_get_bool())) diag_error(node->stmt_if.condition->source_location, "condition is not a boolean");
    LLVMBuildCondBr(context->llvm_builder, condition.llvm_value, bb_then, node->stmt_if.else_body != NULL ? bb_else : bb_end);

    bool then_returned = false;
    bool else_returned = false;

    // Create then, aka body
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_then);
    cg_stmt(context, symtab, scope, node->stmt_if.body);
    then_returned = context->return_state.has_returned;
    context->return_state.has_returned = false;
    if(!then_returned) LLVMBuildBr(context->llvm_builder, bb_end);

    // Create else body
    if(node->stmt_if.else_body != NULL) {
        LLVMAppendExistingBasicBlock(llvm_func, bb_else);
        LLVMPositionBuilderAtEnd(context->llvm_builder, bb_else);
        cg_stmt(context, symtab, scope, node->stmt_if.else_body);
        else_returned = context->return_state.has_returned;
        context->return_state.has_returned = false;
        if(!else_returned) LLVMBuildBr(context->llvm_builder, bb_end);
    }
    context->return_state.has_returned = then_returned && else_returned;

    // Setup end block
    if(then_returned && else_returned) return;
    LLVMAppendExistingBasicBlock(llvm_func, bb_end);
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_end);
}

static void cg_stmt_while(CG_STMT_PARAMS) {
    LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));

    LLVMBasicBlockRef bb_top = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "while.cond");
    LLVMBasicBlockRef bb_then = LLVMCreateBasicBlockInContext(context->llvm_context, "while.then");
    LLVMBasicBlockRef bb_end = LLVMCreateBasicBlockInContext(context->llvm_context, "while.end");

    bool create_end = node->stmt_while.condition != NULL;

    LLVMBuildBr(context->llvm_builder, bb_top);

    // Condition
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_top);
    if(node->stmt_while.condition != NULL) {
        value_t condition = cg_expr(context, symtab, scope, node->stmt_while.condition);
        if(!ir_type_eq(condition.type, ir_type_get_bool())) diag_error(node->stmt_while.condition->source_location, "condition is not a boolean");
        LLVMBuildCondBr(context->llvm_builder, condition.llvm_value, bb_then, bb_end);
    } else {
        LLVMBuildBr(context->llvm_builder, bb_then);
    }

    // Body
    LLVMAppendExistingBasicBlock(llvm_func, bb_then);
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_then);
    cg_stmt(context, symtab, scope, node->stmt_while.body);
    LLVMBuildBr(context->llvm_builder, bb_top);

    // End
    if(!create_end) return;
    LLVMAppendExistingBasicBlock(llvm_func, bb_end);
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_end);
}

// Expressions

static value_t cg_expr_literal_numeric(CG_EXPR_PARAMS) {
    ir_type_t *type = ir_type_get_uint();
    return (value_t) {
        .type = type,
        .llvm_value = LLVMConstInt(ir_type_to_llvm(context, type), node->expr_literal.numeric_value, type->integer.is_signed)
    };
}

static value_t cg_expr_literal_char(CG_EXPR_PARAMS) {
    ir_type_t *type = ir_type_get_char();
    return (value_t) {
        .type = type,
        .llvm_value = LLVMConstInt(ir_type_to_llvm(context, type), node->expr_literal.char_value, false)
    };
}

static value_t cg_expr_literal_string(CG_EXPR_PARAMS) {
    size_t size = strlen(node->expr_literal.string_value) + 1;
    ir_type_t *type = ir_type_array_make(ir_type_get_char(), size);

    LLVMValueRef value = LLVMAddGlobal(context->llvm_module, ir_type_to_llvm(context, type), "expr.literal_string");
    LLVMSetLinkage(value, LLVMInternalLinkage);
    LLVMSetGlobalConstant(value, 1);
    LLVMSetInitializer(value, LLVMConstStringInContext(context->llvm_context, node->expr_literal.string_value, size, true));

    return (value_t) {
        .type = type,
        .llvm_value = value
    };
}

static value_t cg_expr_literal_bool(CG_EXPR_PARAMS) {
    ir_type_t *type = ir_type_get_bool();
    return (value_t) {
        .type = type,
        .llvm_value = LLVMConstInt(ir_type_to_llvm(context, type), node->expr_literal.bool_value, false)
    };
}

static value_t cg_expr_binary(CG_EXPR_PARAMS) {
    value_t right = cg_expr(context, symtab, scope, node->expr_binary.right);
    value_t left = cg_expr_ext(context, symtab, scope, node->expr_binary.left, false);

    ir_type_t *type = right.type;
    if(!ir_type_eq(type, left.type)) diag_error(node->source_location, "conflicting types in binary expression");

    if(node->expr_binary.operation == IR_NODE_BINARY_OPERATION_ASSIGN) {
        if(!left.is_ref) diag_error(node->source_location, "invalid lvalue");
        LLVMBuildStore(context->llvm_builder, right.llvm_value, left.llvm_value);
        return right;
    }

    left = resolve_ref(context, left);
    switch(node->expr_binary.operation) {
        case IR_NODE_BINARY_OPERATION_EQUAL: return (value_t) {
            .type = ir_type_get_bool(),
            .llvm_value = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, left.llvm_value, right.llvm_value, "expr.binary.eq")
        };
        case IR_NODE_BINARY_OPERATION_NOT_EQUAL: return (value_t) {
            .type = ir_type_get_bool(),
            .llvm_value = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, left.llvm_value, right.llvm_value, "expr.binary.ne")
        };
        default: break;
    }

    if(type->kind != IR_TYPE_KIND_INTEGER) diag_error(node->source_location, "invalid type in binary expression");
    switch(node->expr_binary.operation) {
        case IR_NODE_BINARY_OPERATION_ADDITION: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildAdd(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.add")
        };
        case IR_NODE_BINARY_OPERATION_SUBTRACTION: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildSub(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.sub")
        };
        case IR_NODE_BINARY_OPERATION_MULTIPLICATION: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildMul(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.mul")
        };
        case IR_NODE_BINARY_OPERATION_DIVISION: return (value_t) {
            .type = type,
            .llvm_value = type->integer.is_signed ? LLVMBuildSDiv(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.sdiv") : LLVMBuildUDiv(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.udiv")
        };
        case IR_NODE_BINARY_OPERATION_MODULO: return (value_t) {
            .type = type,
            .llvm_value = type->integer.is_signed ? LLVMBuildSRem(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.srem") : LLVMBuildURem(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.urem")
        };
        case IR_NODE_BINARY_OPERATION_GREATER: return (value_t) {
            .type = ir_type_get_bool(),
            .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSGT : LLVMIntUGT, left.llvm_value, right.llvm_value, "expr.binary.gt")
        };
        case IR_NODE_BINARY_OPERATION_GREATER_EQUAL: return (value_t) {
            .type = ir_type_get_bool(),
            .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSGE : LLVMIntUGE, left.llvm_value, right.llvm_value, "expr.binary.ge")
        };
        case IR_NODE_BINARY_OPERATION_LESS: return (value_t) {
            .type = ir_type_get_bool(),
            .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSLT : LLVMIntULT, left.llvm_value, right.llvm_value, "expr.binary.lt")
        };
        case IR_NODE_BINARY_OPERATION_LESS_EQUAL: return (value_t) {
            .type = ir_type_get_bool(),
            .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSLE : LLVMIntULE, left.llvm_value, right.llvm_value, "expr.binary.le")
        };
        default: assert(false);
    }
}

static value_t cg_expr_unary(CG_EXPR_PARAMS) {
    value_t operand = cg_expr_ext(context, symtab, scope, node->expr_unary.operand, false);
    if(node->expr_unary.operation == IR_NODE_UNARY_OPERATION_REF) {
        if(!operand.is_ref) diag_error(node->source_location, "references can only be made to variables");
        return (value_t) {
            .type = ir_type_pointer_make(operand.type),
            .llvm_value = operand.llvm_value
        };
    }

    operand = resolve_ref(context, operand);
    switch(node->expr_unary.operation) {
        case IR_NODE_UNARY_OPERATION_DEREF:
            if(operand.type->kind != IR_TYPE_KIND_POINTER) diag_error(node->source_location, "unary operation \"dereference\" on a non-pointer value");
            ir_type_t *type = operand.type->pointer.referred;
            return (value_t) {
                .is_ref = true,
                .type = type,
                .llvm_value = operand.llvm_value
            };
        case IR_NODE_UNARY_OPERATION_NOT:
            if(operand.type->kind != IR_TYPE_KIND_INTEGER) diag_error(node->source_location, "unary operation \"not\" on a non-numeric value");
            return (value_t) {
                .type = ir_type_get_bool(),
                .llvm_value = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, operand.llvm_value, LLVMConstInt(ir_type_to_llvm(context, operand.type), 0, false), "")
            };
        case IR_NODE_UNARY_OPERATION_NEGATIVE:
            if(operand.type->kind != IR_TYPE_KIND_INTEGER) diag_error(node->source_location, "unary operation \"negative\" on a non-numeric value");
            return (value_t) {
                .type = operand.type,
                .llvm_value = LLVMBuildNeg(context->llvm_builder, operand.llvm_value, "")
            };
        default: assert(false);
    }
}

static value_t cg_expr_variable(CG_EXPR_PARAMS) {
    scope_variable_t *var = scope_variable_find(scope, node->expr_variable.name);
    if(var == NULL) diag_error(node->source_location, "referenced an undefined variable `%s`", node->expr_variable.name);
    return (value_t) {
        .is_ref = true,
        .type = var->type,
        .llvm_value = var->llvm_value
    };
}

static value_t cg_expr_call(CG_EXPR_PARAMS) {
    LLVMValueRef llvm_value;
    ir_type_t *type;

    scope_variable_t *scope_variable = scope_variable_find(scope, node->expr_call.function_name);
    if(scope_variable != NULL) {
        if(scope_variable->type->kind != IR_TYPE_KIND_FUNCTION) goto not_function;
        llvm_value = scope_variable->llvm_value;
        type = scope_variable->type;
        goto found;
    } else {
        symbol_t *symbol = symbol_table_find(symtab, node->expr_call.function_name);
        if(symbol == NULL) {
            symbol = symbol_table_find(&context->root_symtab, node->expr_call.function_name);
            if(symbol == NULL) diag_error(node->source_location, "no such function");
        }
        if(symbol->kind != SYMBOL_KIND_FUNCTION) goto not_function;

        llvm_value = symbol->function.llvm_value;
        type = symbol->function.type;
        goto found;
    }

    not_function: diag_error(node->source_location, "not a function");
    found:

    if(node->expr_call.arguments.count < type->function.argument_count) diag_error(node->source_location, "missing arguments");
    if(!type->function.varargs && node->expr_call.arguments.count > type->function.argument_count) diag_error(node->source_location, "invalid number of arguments");

    size_t argument_count = ir_node_list_count(&node->expr_call.arguments);
    LLVMValueRef arguments[node->expr_call.arguments.count];
    IR_NODE_LIST_FOREACH(&node->expr_call.arguments, {
        value_t argument = cg_expr(context, symtab, scope, node);
        if(type->function.argument_count > i && !ir_type_eq(argument.type, type->function.arguments[i])) diag_error(node->source_location, "argument has invalid type");
        arguments[i] = argument.llvm_value;
    });

    return (value_t) {
        .type = type->function.return_type,
        .llvm_value = LLVMBuildCall2(context->llvm_builder, ir_type_to_llvm(context, type), llvm_value, arguments, argument_count, "")
    };
}

static value_t cg_expr_tuple(CG_EXPR_PARAMS) {
    LLVMValueRef llvm_values[node->expr_tuple.values.count];
    ir_type_t **types = malloc(node->expr_tuple.values.count * sizeof(ir_type_t *));
    IR_NODE_LIST_FOREACH(&node->expr_tuple.values, {
        value_t value = cg_expr(context, symtab, scope, node);
        types[i] = value.type;
        llvm_values[i] = value.llvm_value;
    });

    ir_type_t *type = ir_type_tuple_make(node->expr_tuple.values.count, types);
    LLVMTypeRef llvm_type = ir_type_to_llvm(context, type);

    LLVMValueRef llvm_allocated_tuple = LLVMBuildAlloca(context->llvm_builder, llvm_type, "expr.tuple");
    for(size_t i = 0; i < node->expr_tuple.values.count; i++) {
        LLVMValueRef member_ptr = LLVMBuildStructGEP2(context->llvm_builder, llvm_type, llvm_allocated_tuple, i, "");
        LLVMBuildStore(context->llvm_builder, llvm_values[i], member_ptr);
    }

    return (value_t) {
        .is_ref = true,
        .type = type,
        .llvm_value = llvm_allocated_tuple
    };
}

static value_t cg_expr_cast(CG_EXPR_PARAMS) {
    value_t value_from = cg_expr(context, symtab, scope, node->expr_cast.value);
    if(ir_type_eq(node->expr_cast.type, value_from.type)) return value_from;

    ir_type_t *type_to = node->expr_cast.type;
    ir_type_t *type_from = value_from.type;

    if(type_to->kind == IR_TYPE_KIND_INTEGER && value_from.type->kind == IR_TYPE_KIND_INTEGER) {
        LLVMValueRef value_to;
        if(type_from->integer.bit_size == type_to->integer.bit_size) {
            value_to = value_from.llvm_value;
        } else {
            if(type_from->integer.bit_size > type_to->integer.bit_size) {
                value_to = LLVMBuildTrunc(context->llvm_builder, value_from.llvm_value, ir_type_to_llvm(context, type_to), "cast.trunc");
            } else {
                if(type_to->integer.is_signed) {
                    value_to = LLVMBuildSExt(context->llvm_builder, value_from.llvm_value, ir_type_to_llvm(context, type_to), "cast.sext");
                } else {
                    value_to = LLVMBuildZExt(context->llvm_builder, value_from.llvm_value, ir_type_to_llvm(context, type_to), "cast.zext");
                }
            }
        }
        return (value_t) { .type = type_to, .llvm_value = value_to };
    }

    if(type_from->kind == IR_TYPE_KIND_POINTER && type_to->kind == IR_TYPE_KIND_POINTER) return (value_t) {
        .type = type_to,
        .llvm_value = value_from.llvm_value
    };

    if(type_from->kind == IR_TYPE_KIND_POINTER && type_to->kind == IR_TYPE_KIND_INTEGER) return (value_t) {
        .type = type_to,
        .llvm_value = LLVMBuildPtrToInt(context->llvm_builder, value_from.llvm_value, ir_type_to_llvm(context, type_to), "cast.ptrtoint")
    };

    if(type_from->kind == IR_TYPE_KIND_INTEGER && type_to->kind == IR_TYPE_KIND_POINTER) return (value_t) {
        .type = type_to,
        .llvm_value = LLVMBuildIntToPtr(context->llvm_builder, value_from.llvm_value, ir_type_to_llvm(context, type_to), "cast.inttoptr")
    };

    if(type_from->kind == IR_TYPE_KIND_ARRAY && type_to->kind == IR_TYPE_KIND_POINTER) {
        if(ir_type_eq(type_from->array.type, type_to->pointer.referred)) return (value_t) {
            .type = type_to,
            .llvm_value = value_from.llvm_value // TODO: validate we dont actually need a cast here
        };
    }

    diag_error(node->source_location, "invalid cast");
}

static value_t cg_expr_access_index(CG_EXPR_PARAMS) {
    value_t value = cg_expr_ext(context, symtab, scope, node->expr_access_index.value, false);
    value_t index = cg_expr(context, symtab, scope, node->expr_access_index.index);

    if(index.type->kind != IR_TYPE_KIND_INTEGER) diag_error(node->source_location, "invalid type for indexing");
    if(value.type->kind != IR_TYPE_KIND_ARRAY) diag_error(node->source_location, "can only index arrays");

    return (value_t) {
        .is_ref = true,
        .type = value.type->array.type,
        .llvm_value = LLVMBuildGEP2(context->llvm_builder, ir_type_to_llvm(context, value.type), value.llvm_value, (LLVMValueRef[]) { LLVMConstInt(LLVMInt64TypeInContext(context->llvm_context), 0, false), index.llvm_value }, 2, "expr.access_index")
    };
}

static value_t cg_expr_access_index_const(CG_EXPR_PARAMS) {
    value_t value = cg_expr_ext(context, symtab, scope, node->expr_access_index_const.value, false);

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
    assert(value.is_ref);

    LLVMValueRef member_ptr = LLVMBuildStructGEP2(context->llvm_builder, ir_type_to_llvm(context, value.type), value.llvm_value, node->expr_access_index_const.index, "expr.access_index_const");
    return (value_t) {
        .is_ref = true,
        .type = member_type,
        .llvm_value = member_ptr
    };
}

static value_t cg_expr_selector(CG_EXPR_PARAMS) {
    symbol_t *symbol = symbol_table_find_kind(symtab, node->expr_selector.name, SYMBOL_KIND_MODULE);
    if(symbol == NULL) diag_error(node->source_location, "unknown module `%s`", node->expr_selector.name);
    return cg_expr_ext(context, &symbol->module.symtab, scope, node->expr_selector.value, false);
}

// Common

static void cg_root(CG_TLC_PARAMS) {
    assert(node->type == IR_NODE_TYPE_ROOT);
    IR_NODE_LIST_FOREACH(&node->root.tlcs, cg_tlc(context, symtab, node));
}

static void cg_tlc(CG_TLC_PARAMS) {
    switch(node->type) {
        case IR_NODE_TYPE_TLC_MODULE: cg_tlc_module(context, symtab, node); break;
        case IR_NODE_TYPE_TLC_FUNCTION: cg_tlc_function(context, symtab, node); break;
        case IR_NODE_TYPE_TLC_EXTERN: break;

        case IR_NODE_TYPE_ROOT: assert(false);
        CASE_STMT(assert(false));
        CASE_EXPRESSION(assert(false));
    }
}

static void cg_stmt(CG_STMT_PARAMS) {
    switch(node->type) {
        case IR_NODE_TYPE_STMT_NOOP: break;
        case IR_NODE_TYPE_STMT_BLOCK: cg_stmt_block(context, symtab, scope, node); break;
        case IR_NODE_TYPE_STMT_DECLARATION: cg_stmt_declaration(context, symtab, scope, node); break;
        case IR_NODE_TYPE_STMT_EXPRESSION: cg_expr(context, symtab, scope, node->stmt_expression.expression); break;
        case IR_NODE_TYPE_STMT_RETURN: cg_stmt_return(context, symtab, scope, node); break;
        case IR_NODE_TYPE_STMT_IF: cg_stmt_if(context, symtab, scope, node); break;
        case IR_NODE_TYPE_STMT_WHILE: cg_stmt_while(context, symtab, scope, node); break;

        case IR_NODE_TYPE_ROOT: assert(false);
        CASE_TLC(assert(false));
        CASE_EXPRESSION(assert(false));
    }
}

static value_t cg_expr(CG_EXPR_PARAMS) {
    return cg_expr_ext(context, symtab, scope, node, true);
}

static value_t cg_expr_ext(CG_EXPR_PARAMS, bool do_resolve_ref) {
    value_t value;
    switch(node->type) {
        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: value = cg_expr_literal_numeric(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: value = cg_expr_literal_string(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: value = cg_expr_literal_char(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: value = cg_expr_literal_bool(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_BINARY: value = cg_expr_binary(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_UNARY: value = cg_expr_unary(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_VARIABLE: value = cg_expr_variable(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_CALL: value = cg_expr_call(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_TUPLE: value = cg_expr_tuple(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_CAST: value = cg_expr_cast(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX: value = cg_expr_access_index(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX_CONST: value = cg_expr_access_index_const(context, symtab, scope, node); break;
        case IR_NODE_TYPE_EXPR_SELECTOR: value = cg_expr_selector(context, symtab, scope, node); break;

        case IR_NODE_TYPE_ROOT: assert(false);
        CASE_TLC(assert(false));
        CASE_STMT(assert(false));
    }
    if(do_resolve_ref) return resolve_ref(context, value);
    return value;
}

void codegen(ir_node_t *root_node, const char *path, const char *passes) {}

void codegen_ir(ir_node_t *root_node, const char *path) {
    context_t context;
    context.llvm_context = LLVMContextCreate();
    context.llvm_builder = LLVMCreateBuilderInContext(context.llvm_context);
    context.llvm_module = LLVMModuleCreateWithNameInContext("CharonModule", context.llvm_context);
    context.root_symtab = SYMBOL_TABLE_INIT;

    gst(&context, &context.root_symtab, root_node);
    cg_root(&context, &context.root_symtab, root_node);

    char *error_message;
    LLVMBool failure = LLVMPrintModuleToFile(context.llvm_module, path, &error_message);
    if(failure) log_fatal("emit failed (%s)\n", error_message);

    LLVMDisposeBuilder(context.llvm_builder);
    LLVMDisposeModule(context.llvm_module);
    LLVMContextDispose(context.llvm_context);
}