#include "codegen.h"

#include "constants.h"
#include "lib/diag.h"
#include "lib/alloc.h"
#include "lib/log.h"
#include "llir/symbol.h"

#include <string.h>

#define LLIR_TYPE_BOOL llir_type_cache_get_integer(context->anon_type_cache, 1, false, false)

typedef struct {
    LLVMContextRef llvm_context;
    LLVMBuilderRef llvm_builder;
    LLVMModuleRef llvm_module;

    llir_namespace_t *root_namespace;
    llir_type_cache_t *anon_type_cache;

    struct {
        llir_type_t *type;
        bool has_returned;
        bool wont_return;
    } return_state;

    struct {
        bool in_loop, has_terminated, can_break;
        LLVMBasicBlockRef bb_beginning, bb_end;
    } loop_state;
} context_t;

typedef struct scope scope_t;

typedef struct {
    scope_t *scope;
    const char *name;
    llir_type_t *type;
    LLVMValueRef llvm_value;
} scope_variable_t;

struct scope {
    scope_t *parent;
    scope_variable_t *variables;
    size_t variable_count;
};

typedef struct {
    bool is_ref;
    llir_type_t *type;
    LLVMValueRef llvm_value;
} value_t;

static char *mangle_name(const char *name, llir_symbol_t *parent) {
    char *mangled_name = alloc_strdup(name);
    while(parent != NULL) {
        assert(parent->kind == LLIR_SYMBOL_KIND_MODULE);

        size_t name_length = strlen(mangled_name);
        size_t module_name_length= strlen(parent->name);
        char *new = alloc(module_name_length + 2 + name_length + 1);
        memcpy(new, parent->name, module_name_length);
        memset(&new[module_name_length], ':', 2);
        memcpy(&new[module_name_length + 2], mangled_name, name_length);
        new[module_name_length + 2 + name_length] = '\0';

        alloc_free(mangled_name);
        mangled_name = new;
        parent = parent->namespace->parent;
    }
    return mangled_name;
}


static LLVMTypeRef llir_type_to_llvm(context_t *context, llir_type_t *type) {
    assert(type != NULL);
    switch(type->kind) {
        case LLIR_TYPE_KIND_VOID: return LLVMVoidTypeInContext(context->llvm_context);
        case LLIR_TYPE_KIND_INTEGER:
            switch(type->integer.bit_size) {
                case 1: return LLVMInt1TypeInContext(context->llvm_context);
                case 8: return LLVMInt8TypeInContext(context->llvm_context);
                case 16: return LLVMInt16TypeInContext(context->llvm_context);
                case 32: return LLVMInt32TypeInContext(context->llvm_context);
                case 64: return LLVMInt64TypeInContext(context->llvm_context);
                default: assert(false);
            }
        case LLIR_TYPE_KIND_POINTER: return LLVMPointerTypeInContext(context->llvm_context, 0);
        case LLIR_TYPE_KIND_ARRAY: return LLVMArrayType2(llir_type_to_llvm(context, type->array.type), type->array.size);
        case LLIR_TYPE_KIND_TUPLE: {
            LLVMTypeRef types[type->tuple.type_count];
            for(size_t i = 0; i < type->tuple.type_count; i++) types[i] = llir_type_to_llvm(context, type->tuple.types[i]);
            return LLVMStructTypeInContext(context->llvm_context, types, type->tuple.type_count, false);
        }
        case LLIR_TYPE_KIND_STRUCTURE: {
            LLVMTypeRef types[type->structure.member_count];
            for(size_t i = 0; i < type->structure.member_count; i++) types[i] = llir_type_to_llvm(context, type->structure.members[i].type);
            return LLVMStructTypeInContext(context->llvm_context, types, type->structure.member_count, type->structure.packed);
        }
        case LLIR_TYPE_KIND_FUNCTION_REFERENCE: return LLVMPointerTypeInContext(context->llvm_context, 0);
        case LLIR_TYPE_KIND_ENUMERATION: return LLVMIntTypeInContext(context->llvm_context, type->enumeration.bit_size);
    }
    assert(false);
}

static LLVMTypeRef llir_type_function_to_llvm(context_t *context, llir_type_function_t *function_type) {
    LLVMTypeRef fn_return_type = llir_type_to_llvm(context, function_type->return_type);
    if(function_type->argument_count > 0) {
        LLVMTypeRef fn_arguments[function_type->argument_count];
        for(size_t i = 0; i < function_type->argument_count; i++) {
            fn_arguments[i] = llir_type_to_llvm(context, function_type->arguments[i]);
        }
        return LLVMFunctionType(fn_return_type, fn_arguments, function_type->argument_count, function_type->varargs);
    }
    return LLVMFunctionType(fn_return_type, NULL, 0, function_type->varargs);
}

static value_t resolve_ref(context_t *context, value_t value) {
    if(!value.is_ref) return value;
    LLVMValueRef temp = LLVMBuildLoad2(context->llvm_builder, llir_type_to_llvm(context, value.type), value.llvm_value, "resolve_ref");
    return (value_t) {
        .type = value.type,
        .llvm_value = temp
    };
}

static bool try_coerce(context_t *context, llir_type_t *to_type, value_t *value) {
    if(llir_type_eq(to_type, value->type)) return true;

    if(to_type->kind == LLIR_TYPE_KIND_INTEGER) {
        if(value->type->kind != LLIR_TYPE_KIND_INTEGER) return false;
        if(value->type->integer.is_signed != to_type->integer.is_signed) return false;
        if(value->type->integer.bit_size > to_type->integer.bit_size) return false;

        *value = resolve_ref(context, *value);

        value->type = to_type;
        if(to_type->integer.is_signed) {
            value->llvm_value = LLVMBuildSExt(context->llvm_builder, value->llvm_value, llir_type_to_llvm(context, to_type), "coerce.sext");
        } else {
            value->llvm_value = LLVMBuildZExt(context->llvm_builder, value->llvm_value, llir_type_to_llvm(context, to_type), "coerce.zext");
        }
        return true;
    }

    if(to_type->kind == LLIR_TYPE_KIND_POINTER) {
        switch(value->type->kind) {
            case LLIR_TYPE_KIND_ARRAY:
                if(!llir_type_eq(to_type->pointer.pointee, value->type->array.type)) return false;
                break;
            case LLIR_TYPE_KIND_INTEGER:
                if(!value->type->integer.allow_coerce_pointer) return false;
                value->llvm_value = LLVMBuildIntToPtr(context->llvm_builder, value->llvm_value, llir_type_to_llvm(context, to_type), "coerce.inttoptr");
                break;
            default: return false;
        }
        value->type = to_type;
        return true;
    }

    return false;
}

static scope_t *scope_enter(scope_t *current) {
    scope_t *scope = alloc(sizeof(scope_t));
    scope->parent = current;
    scope->variable_count = 0;
    scope->variables = NULL;
    return scope;
}

static scope_t *scope_exit(scope_t *current) {
    scope_t *parent = current->parent;
    alloc_free(current->variables);
    alloc_free(current);
    return parent;
}

static void scope_variable_add(scope_t *scope, const char *name, llir_type_t *type, LLVMValueRef llvm_value) {
    scope->variables = alloc_array(scope->variables, ++scope->variable_count, sizeof(scope_variable_t));
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

// //
// Codegen
//

#define CG_TLC_PARAMS context_t *context, llir_namespace_t *current_namespace, llir_node_t *node
#define CG_STMT_PARAMS context_t *context, llir_namespace_t *current_namespace, scope_t *scope, llir_node_t *node
#define CG_EXPR_PARAMS context_t *context, llir_namespace_t *current_namespace, scope_t *scope, llir_node_t *node

static void cg_tlc(CG_TLC_PARAMS);
static void cg_stmt(CG_STMT_PARAMS);
static value_t cg_expr(CG_EXPR_PARAMS);
static value_t cg_expr_ext(CG_EXPR_PARAMS, bool do_resolve_ref);

// TLCs

static void cg_tlc_module(CG_TLC_PARAMS) {
    llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_function.name, LLIR_SYMBOL_KIND_MODULE);
    assert(symbol != NULL);

    LLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, cg_tlc(context, symbol->module.namespace, node));
}

static void cg_tlc_function(CG_TLC_PARAMS) {
    llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_function.name, LLIR_SYMBOL_KIND_FUNCTION);
    assert(symbol != NULL);

    context->return_state.has_returned = false;
    context->return_state.wont_return = false;
    context->return_state.type = symbol->function.function_type->return_type;
    context->loop_state.in_loop = false;

    scope_t *scope = scope_enter(NULL);

    LLVMBasicBlockRef bb_entry = LLVMAppendBasicBlockInContext(context->llvm_context, symbol->function.codegen_data, "tlc.function");
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_entry);

    for(size_t i = 0; i < symbol->function.function_type->argument_count; i++) {
        llir_type_t *param_type = symbol->function.function_type->arguments[i];
        const char *param_name = node->tlc_function.argument_names[i];

        LLVMValueRef param = LLVMBuildAlloca(context->llvm_builder, llir_type_to_llvm(context, param_type), param_name);
        LLVMBuildStore(context->llvm_builder, LLVMGetParam(symbol->function.codegen_data, i), param);
        scope_variable_add(scope, param_name, param_type, param);
    }

    cg_stmt(context, current_namespace, scope, node->tlc_function.statement);

    if(!context->return_state.has_returned && !context->return_state.wont_return) {
        if(context->return_state.type->kind != LLIR_TYPE_KIND_VOID) diag_error(node->source_location, "missing return");
        LLVMBuildRetVoid(context->llvm_builder);
    }

    scope_exit(scope);
}

// Statements

static void cg_stmt_block(CG_STMT_PARAMS) {
    scope = scope_enter(scope);
    LLIR_NODE_LIST_FOREACH(&node->stmt_block.statements, {
        cg_stmt(context, current_namespace, scope, node);
        if((context->return_state.has_returned || context->return_state.wont_return) || (context->loop_state.in_loop && context->loop_state.has_terminated)) break;
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
    llir_type_t *type = node->stmt_declaration.type;
    if(node->stmt_declaration.initial != NULL) {
        value_t value = cg_expr(context, current_namespace, scope, node->stmt_declaration.initial);
        if(type == NULL) {
            type = value.type;
        } else {
            if(!try_coerce(context, type, &value)) diag_error(node->source_location, "declarations initial value does not match its explicit type");
        }
        llvm_value = value.llvm_value;
    }
    if(type == NULL) diag_error(node->source_location, "declaration is missing both an explicit and inferred type");

    LLVMBasicBlockRef bb_entry = LLVMGetEntryBasicBlock(func_parent);
    LLVMBuilderRef entry_builder = LLVMCreateBuilderInContext(context->llvm_context);
    LLVMPositionBuilder(entry_builder, bb_entry, LLVMGetFirstInstruction(bb_entry));
    LLVMValueRef ref = LLVMBuildAlloca(entry_builder, llir_type_to_llvm(context, type), node->stmt_declaration.name);
    LLVMDisposeBuilder(entry_builder);
    if(llvm_value != NULL) LLVMBuildStore(context->llvm_builder, llvm_value, ref);

    scope_variable_add(scope, node->stmt_declaration.name, type, ref);
}

static void cg_stmt_return(CG_STMT_PARAMS) {
    context->return_state.has_returned = true;
    if(context->return_state.type->kind == LLIR_TYPE_KIND_VOID) {
        if(node->stmt_return.value) {
            value_t value = cg_expr(context, current_namespace, scope, node->stmt_return.value);
            if(value.type->kind == LLIR_TYPE_KIND_VOID) diag_error(node->source_location, "value returned from a function without a return type");
        }
        LLVMBuildRetVoid(context->llvm_builder);
        return;
    }
    if(node->stmt_return.value == NULL) diag_error(node->source_location, "no return value");
    value_t value = cg_expr(context, current_namespace, scope, node->stmt_return.value);
    if(!try_coerce(context, context->return_state.type, &value)) diag_error(node->source_location, "invalid type");
    LLVMBuildRet(context->llvm_builder, value.llvm_value);
}

static void cg_stmt_if(CG_STMT_PARAMS) {
    LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));
    LLVMBasicBlockRef bb_body = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "if.body");
    LLVMBasicBlockRef bb_else_body = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "if.else_body");
    LLVMBasicBlockRef bb_end = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "if.end");

    value_t condition = cg_expr(context, current_namespace, scope, node->stmt_if.condition);
    if(!llir_type_eq(condition.type, LLIR_TYPE_BOOL)) diag_error(node->stmt_if.condition->source_location, "condition is not a boolean");
    LLVMBuildCondBr(context->llvm_builder, condition.llvm_value, bb_body, node->stmt_if.else_body != NULL ? bb_else_body : bb_end);

    // Create then, aka body
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_body);
    cg_stmt(context, current_namespace, scope, node->stmt_if.body);
    bool then_wont_return = context->return_state.wont_return;
    bool then_returned = context->return_state.has_returned;
    bool then_terminated = context->loop_state.in_loop && context->loop_state.has_terminated;
    if(!then_wont_return && !then_returned && !then_terminated) LLVMBuildBr(context->llvm_builder, bb_end);

    // Create else body
    bool else_wont_return = false, else_returned = false, else_terminated = false;
    if(node->stmt_if.else_body != NULL) {
        context->return_state.has_returned = false;
        context->loop_state.has_terminated = false;

        LLVMPositionBuilderAtEnd(context->llvm_builder, bb_else_body);
        cg_stmt(context, current_namespace, scope, node->stmt_if.else_body);
        else_wont_return = context->return_state.wont_return;
        else_returned = context->return_state.has_returned;
        else_terminated = context->loop_state.in_loop && context->loop_state.has_terminated;
        if(!else_wont_return && !else_returned && !else_terminated) LLVMBuildBr(context->llvm_builder, bb_end);
    } else {
        LLVMDeleteBasicBlock(bb_else_body);
    }
    context->return_state.wont_return = then_wont_return && else_wont_return;
    context->return_state.has_returned = then_returned && else_returned;
    context->loop_state.has_terminated = then_terminated && else_terminated;

    // Setup end block
    if(context->return_state.wont_return || context->return_state.has_returned || context->loop_state.has_terminated) {
        LLVMDeleteBasicBlock(bb_end);
    } else {
        LLVMPositionBuilderAtEnd(context->llvm_builder, bb_end);
    }
}

static void cg_stmt_while(CG_STMT_PARAMS) {
    LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));
    LLVMBasicBlockRef bb_top = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "while.cond");
    LLVMBasicBlockRef bb_body = LLVMCreateBasicBlockInContext(context->llvm_context, "while.body");
    LLVMBasicBlockRef bb_end = LLVMCreateBasicBlockInContext(context->llvm_context, "while.end");

    bool create_end = node->stmt_while.condition != NULL;

    LLVMBuildBr(context->llvm_builder, bb_top);

    // Condition
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_top);
    if(node->stmt_while.condition != NULL) {
        value_t condition = cg_expr(context, current_namespace, scope, node->stmt_while.condition);
        if(!llir_type_eq(condition.type, LLIR_TYPE_BOOL)) diag_error(node->stmt_while.condition->source_location, "condition is not a boolean");
        LLVMBuildCondBr(context->llvm_builder, condition.llvm_value, bb_body, bb_end);
    } else {
        LLVMBuildBr(context->llvm_builder, bb_body);
    }

    // Body
    LLVMAppendExistingBasicBlock(llvm_func, bb_body);
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_body);

    typeof(context->loop_state) prev = context->loop_state;
    context->loop_state.in_loop = true;
    context->loop_state.has_terminated = false;
    context->loop_state.can_break = false;
    context->loop_state.bb_beginning = bb_top;
    context->loop_state.bb_end = bb_end;

    if(node->stmt_while.body != NULL) cg_stmt(context, current_namespace, scope, node->stmt_while.body);
    if(!context->loop_state.has_terminated) LLVMBuildBr(context->llvm_builder, bb_top);
    if(context->loop_state.can_break) create_end = true;
    context->loop_state = prev;

    // End
    if(create_end) {
        LLVMAppendExistingBasicBlock(llvm_func, bb_end);
        LLVMPositionBuilderAtEnd(context->llvm_builder, bb_end);
    } else {
        context->return_state.wont_return = true;
    }
}

static void cg_stmt_continue(CG_STMT_PARAMS) {
    if(!context->loop_state.in_loop) diag_error(node->source_location, "continue out of loop");
    context->loop_state.has_terminated = true;
    LLVMBuildBr(context->llvm_builder, context->loop_state.bb_beginning);
}

static void cg_stmt_break(CG_STMT_PARAMS) {
    if(!context->loop_state.in_loop) diag_error(node->source_location, "break out of loop");
    context->loop_state.has_terminated = true;
    context->loop_state.can_break = true;
    LLVMBuildBr(context->llvm_builder, context->loop_state.bb_end);
}

static void cg_stmt_for(CG_STMT_PARAMS) {
    LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));
    LLVMBasicBlockRef bb_top = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "while.cond");
    LLVMBasicBlockRef bb_body = LLVMCreateBasicBlockInContext(context->llvm_context, "while.body");
    LLVMBasicBlockRef bb_end = LLVMCreateBasicBlockInContext(context->llvm_context, "while.end");

    bool create_end = node->stmt_for.condition != NULL;

    scope = scope_enter(scope);

    if(node->stmt_for.declaration != NULL) cg_stmt(context, current_namespace, scope, node->stmt_for.declaration);

    LLVMBuildBr(context->llvm_builder, bb_top);

    // Condition
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_top);
    if(node->stmt_for.condition != NULL) {
        value_t condition = cg_expr(context, current_namespace, scope, node->stmt_for.condition);
        if(!llir_type_eq(condition.type, LLIR_TYPE_BOOL)) diag_error(node->stmt_for.condition->source_location, "condition is not a boolean");
        LLVMBuildCondBr(context->llvm_builder, condition.llvm_value, bb_body, bb_end);
    } else {
        LLVMBuildBr(context->llvm_builder, bb_body);
    }

    // Body
    LLVMAppendExistingBasicBlock(llvm_func, bb_body);
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_body);

    typeof(context->loop_state) prev = context->loop_state;
    context->loop_state.in_loop = true;
    context->loop_state.has_terminated = false;
    context->loop_state.can_break = false;
    context->loop_state.bb_beginning = bb_top;
    context->loop_state.bb_end = bb_end;

    if(node->stmt_for.body != NULL) cg_stmt(context, current_namespace, scope, node->stmt_for.body);
    if(node->stmt_for.expr_after != NULL) cg_expr(context, current_namespace, scope, node->stmt_for.expr_after);
    if(!context->loop_state.has_terminated) LLVMBuildBr(context->llvm_builder, bb_top);
    if(context->loop_state.can_break) create_end = true;
    context->loop_state = prev;

    // End
    if(create_end) {
        LLVMAppendExistingBasicBlock(llvm_func, bb_end);
        LLVMPositionBuilderAtEnd(context->llvm_builder, bb_end);
    } else {
        context->return_state.wont_return = true;
    }
    scope = scope_exit(scope);
}

// static void cg_stmt_assembly(CG_STMT_PARAMS) {
//     LLVMValueRef asm_string = LLVMGetInlineAsm(llir_type_to_llvm(context, llir_type_cache_get_void(context->anon_type_cache)), "", 0, "", 0, true, false, LLVMInlineAsmDialectATT, false);

// }

// Expressions

static value_t cg_expr_literal_numeric(CG_EXPR_PARAMS) {
    llir_type_t *type = llir_type_cache_get_integer(context->anon_type_cache, CONSTANTS_INT_SIZE, false, false);
    return (value_t) {
        .type = type, // TODO: signed?
        .llvm_value = LLVMConstInt(llir_type_to_llvm(context, type), node->expr.literal.numeric_value, type->integer.is_signed)
    };
}

static value_t cg_expr_literal_char(CG_EXPR_PARAMS) {
    llir_type_t *type = llir_type_cache_get_integer(context->anon_type_cache, CONSTANTS_CHAR_SIZE, false, false);
    return (value_t) {
        .type = type,
        .llvm_value = LLVMConstInt(llir_type_to_llvm(context, type), node->expr.literal.char_value, false)
    };
}

static value_t cg_expr_literal_string(CG_EXPR_PARAMS) {
    size_t size = strlen(node->expr.literal.string_value) + 1;
    llir_type_t *type = llir_type_cache_get_array(context->anon_type_cache, llir_type_cache_get_integer(context->anon_type_cache, CONSTANTS_CHAR_SIZE, false, false), size);

    LLVMValueRef value = LLVMAddGlobal(context->llvm_module, llir_type_to_llvm(context, type), "expr.literal_string");
    LLVMSetLinkage(value, LLVMInternalLinkage);
    LLVMSetGlobalConstant(value, 1);
    LLVMSetInitializer(value, LLVMConstStringInContext(context->llvm_context, node->expr.literal.string_value, size, true));

    return (value_t) {
        .type = type,
        .llvm_value = value
    };
}

static value_t cg_expr_literal_bool(CG_EXPR_PARAMS) {
    llir_type_t *type = LLIR_TYPE_BOOL;
    return (value_t) {
        .type = type,
        .llvm_value = LLVMConstInt(llir_type_to_llvm(context, type), node->expr.literal.bool_value, false)
    };
}

static value_t cg_expr_binary(CG_EXPR_PARAMS) {
    switch(node->expr.binary.operation) {
        enum { LOGICAL_AND, LOGICAL_OR } op;
        case LLIR_NODE_BINARY_OPERATION_LOGICAL_AND: op = LOGICAL_AND; goto logical;
        case LLIR_NODE_BINARY_OPERATION_LOGICAL_OR: op = LOGICAL_OR; goto logical;
        logical: {
            LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));

            LLVMBasicBlockRef bb_phi = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "expr.binary.logical.phi");
            LLVMBasicBlockRef bb_end = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "expr.binary.logical.end");

            LLVMBasicBlockRef blocks[2];

            // Left
            value_t left = cg_expr(context, current_namespace, scope, node->expr.binary.left);
            if(!llir_type_eq(left.type, LLIR_TYPE_BOOL)) diag_error(node->expr.binary.left->source_location, "condition is not a boolean");
            LLVMBuildCondBr(context->llvm_builder, left.llvm_value, op == LOGICAL_OR ? bb_phi : bb_end, op == LOGICAL_OR ? bb_end : bb_phi);
            blocks[0] = LLVMGetInsertBlock(context->llvm_builder);

            // Right
            LLVMPositionBuilderAtEnd(context->llvm_builder, bb_end);
            value_t right = cg_expr(context, current_namespace, scope, node->expr.binary.right);
            if(!llir_type_eq(right.type, LLIR_TYPE_BOOL)) diag_error(node->expr.binary.right->source_location, "condition is not a boolean");
            LLVMBuildBr(context->llvm_builder, bb_phi);
            blocks[1] = LLVMGetInsertBlock(context->llvm_builder);

            // Phi
            LLVMPositionBuilderAtEnd(context->llvm_builder, bb_phi);
            LLVMValueRef phi = LLVMBuildPhi(context->llvm_builder, llir_type_to_llvm(context, LLIR_TYPE_BOOL), "expr.binary.logical.value");
            LLVMAddIncoming(phi, (LLVMValueRef[]) { left.llvm_value, right.llvm_value }, blocks, 2);

            return (value_t) { .type = LLIR_TYPE_BOOL, .llvm_value = phi };
        }
        default: break;
    }

    value_t right = cg_expr(context, current_namespace, scope, node->expr.binary.right);
    value_t left = cg_expr_ext(context, current_namespace, scope, node->expr.binary.left, false);

    if(node->expr.binary.operation == LLIR_NODE_BINARY_OPERATION_ASSIGN) {
        if(!try_coerce(context, left.type, &right)) diag_error(node->source_location, "conflicting types in assignment");
        if(!left.is_ref) diag_error(node->source_location, "invalid lvalue");
        LLVMBuildStore(context->llvm_builder, right.llvm_value, left.llvm_value);
        return right;
    }

    left = resolve_ref(context, left);
    llir_type_t *type = left.type;
    if(!try_coerce(context, type, &right)) {
        type = right.type;
        if(!try_coerce(context, type, &left)) diag_error(node->source_location, "conflicting types in binary expression");
    }

    switch(node->expr.binary.operation) {
        case LLIR_NODE_BINARY_OPERATION_EQUAL: return (value_t) {
            .type = LLIR_TYPE_BOOL,
            .llvm_value = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, left.llvm_value, right.llvm_value, "expr.binary.eq")
        };
        case LLIR_NODE_BINARY_OPERATION_NOT_EQUAL: return (value_t) {
            .type = LLIR_TYPE_BOOL,
            .llvm_value = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, left.llvm_value, right.llvm_value, "expr.binary.ne")
        };
        default: break;
    }

    if(type->kind != LLIR_TYPE_KIND_INTEGER) diag_error(node->source_location, "invalid type in binary expression");
    switch(node->expr.binary.operation) {
        case LLIR_NODE_BINARY_OPERATION_ADDITION: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildAdd(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.add")
        };
        case LLIR_NODE_BINARY_OPERATION_SUBTRACTION: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildSub(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.sub")
        };
        case LLIR_NODE_BINARY_OPERATION_MULTIPLICATION: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildMul(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.mul")
        };
        case LLIR_NODE_BINARY_OPERATION_DIVISION: return (value_t) {
            .type = type,
            .llvm_value = type->integer.is_signed ? LLVMBuildSDiv(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.sdiv") : LLVMBuildUDiv(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.udiv")
        };
        case LLIR_NODE_BINARY_OPERATION_MODULO: return (value_t) {
            .type = type,
            .llvm_value = type->integer.is_signed ? LLVMBuildSRem(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.srem") : LLVMBuildURem(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.urem")
        };
        case LLIR_NODE_BINARY_OPERATION_GREATER: return (value_t) {
            .type = LLIR_TYPE_BOOL,
            .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSGT : LLVMIntUGT, left.llvm_value, right.llvm_value, "expr.binary.gt")
        };
        case LLIR_NODE_BINARY_OPERATION_GREATER_EQUAL: return (value_t) {
            .type = LLIR_TYPE_BOOL,
            .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSGE : LLVMIntUGE, left.llvm_value, right.llvm_value, "expr.binary.ge")
        };
        case LLIR_NODE_BINARY_OPERATION_LESS: return (value_t) {
            .type = LLIR_TYPE_BOOL,
            .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSLT : LLVMIntULT, left.llvm_value, right.llvm_value, "expr.binary.lt")
        };
        case LLIR_NODE_BINARY_OPERATION_LESS_EQUAL: return (value_t) {
            .type = LLIR_TYPE_BOOL,
            .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSLE : LLVMIntULE, left.llvm_value, right.llvm_value, "expr.binary.le")
        };
        case LLIR_NODE_BINARY_OPERATION_SHIFT_LEFT: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildShl(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.shl")
        };
        case LLIR_NODE_BINARY_OPERATION_SHIFT_RIGHT: return (value_t) {
            .type = type,
            .llvm_value = type->integer.is_signed ? LLVMBuildAShr(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.ashr") : LLVMBuildLShr(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.lshr")
        };
        case LLIR_NODE_BINARY_OPERATION_AND: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildAnd(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.and")
        };
        case LLIR_NODE_BINARY_OPERATION_OR: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildOr(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.or")
        };
        case LLIR_NODE_BINARY_OPERATION_XOR: return (value_t) {
            .type = type,
            .llvm_value = LLVMBuildXor(context->llvm_builder, left.llvm_value, right.llvm_value, "expr.binary.xor")
        };
        default: assert(false);
    }
}

static value_t cg_expr_unary(CG_EXPR_PARAMS) {
    value_t operand = cg_expr_ext(context, current_namespace, scope, node->expr.unary.operand, false);
    if(node->expr.unary.operation == LLIR_NODE_UNARY_OPERATION_REF) {
        if(!operand.is_ref) diag_error(node->source_location, "references can only be made to variables");

        llir_type_t *type = llir_type_cache_get_pointer(context->anon_type_cache, operand.type);
        return (value_t) {
            .type = type,
            .llvm_value = operand.llvm_value
        };
    }

    operand = resolve_ref(context, operand);
    switch(node->expr.unary.operation) {
        case LLIR_NODE_UNARY_OPERATION_DEREF:
            if(operand.type->kind != LLIR_TYPE_KIND_POINTER) diag_error(node->source_location, "unary operation \"dereference\" on a non-pointer value");
            llir_type_t *type = operand.type->pointer.pointee;
            return (value_t) {
                .is_ref = true,
                .type = type,
                .llvm_value = operand.llvm_value
            };
        case LLIR_NODE_UNARY_OPERATION_NOT:
            if(operand.type->kind != LLIR_TYPE_KIND_INTEGER) diag_error(node->source_location, "unary operation \"not\" on a non-numeric value");
            return (value_t) {
                .type = LLIR_TYPE_BOOL,
                .llvm_value = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, operand.llvm_value, LLVMConstInt(llir_type_to_llvm(context, operand.type), 0, false), "")
            };
        case LLIR_NODE_UNARY_OPERATION_NEGATIVE:
            if(operand.type->kind != LLIR_TYPE_KIND_INTEGER) diag_error(node->source_location, "unary operation \"negative\" on a non-numeric value");
            return (value_t) {
                .type = operand.type,
                .llvm_value = LLVMBuildNeg(context->llvm_builder, operand.llvm_value, "")
            };
        default: assert(false);
    }
}

static value_t cg_expr_variable(CG_EXPR_PARAMS) {
    LLVMValueRef llvm_value;
    llir_type_t *type;
    bool is_ref;

    scope_variable_t *scope_variable = scope_variable_find(scope, node->expr.variable.name);
    if(scope_variable != NULL) {
        llvm_value = scope_variable->llvm_value;
        type = scope_variable->type;
        is_ref = true;
        goto found;
    } else {
        llir_symbol_t *symbol = llir_namespace_find_symbol(current_namespace, node->expr.variable.name);
        if(symbol == NULL) {
            symbol = llir_namespace_find_symbol(context->root_namespace, node->expr.variable.name);
            if(symbol == NULL) goto not_found;
        }
        switch(symbol->kind) {
            case LLIR_SYMBOL_KIND_VARIABLE:
                llvm_value = symbol->variable.codegen_data;
                type = symbol->variable.type;
                is_ref = true;
                break;
            case LLIR_SYMBOL_KIND_FUNCTION:
                llvm_value = symbol->function.codegen_data;
                type = llir_type_cache_get_function_reference(context->anon_type_cache, symbol->function.function_type);
                is_ref = false;
                break;
            default: goto not_found;
        }

        goto found;
    }

    not_found: diag_error(node->source_location, "referenced an undefined variable `%s`", node->expr.variable.name);
    found:

    return (value_t) {
        .is_ref = is_ref,
        .type = type,
        .llvm_value = llvm_value
    };
}

static value_t cg_expr_call(CG_EXPR_PARAMS) {
    value_t value = cg_expr(context, current_namespace, scope, node->expr.call.function_reference);
    if(value.type->kind != LLIR_TYPE_KIND_FUNCTION_REFERENCE) diag_error(node->source_location, "not a function");

    llir_type_function_t *fn_type = value.type->function_reference.function_type;

    if(node->expr.call.arguments.count < fn_type->argument_count) diag_error(node->source_location, "missing arguments");
    if(!fn_type->varargs && node->expr.call.arguments.count > fn_type->argument_count) diag_error(node->source_location, "invalid number of arguments");

    LLVMValueRef llvm_value;
    size_t argument_count = llir_node_list_count(&node->expr.call.arguments);
    if(argument_count > 0) {
        LLVMValueRef arguments[node->expr.call.arguments.count];
        LLIR_NODE_LIST_FOREACH(&node->expr.call.arguments, {
            value_t argument = cg_expr_ext(context, current_namespace, scope, node, false);
            if(fn_type->argument_count > i && !try_coerce(context, fn_type->arguments[i], &argument)) diag_error(node->source_location, "argument has invalid type");
            argument = resolve_ref(context, argument);
            arguments[i] = argument.llvm_value;
        });
        llvm_value = LLVMBuildCall2(context->llvm_builder, llir_type_function_to_llvm(context, fn_type), value.llvm_value, arguments, argument_count, "");
    } else {
        llvm_value = LLVMBuildCall2(context->llvm_builder, llir_type_function_to_llvm(context, fn_type), value.llvm_value, NULL, 0, "");
    }

    return (value_t) {
        .type = fn_type->return_type,
        .llvm_value = llvm_value
    };
}

static value_t cg_expr_tuple(CG_EXPR_PARAMS) {
    LLVMValueRef llvm_values[node->expr.tuple.values.count];
    llir_type_t **types = alloc(node->expr.tuple.values.count * sizeof(llir_type_t *));
    LLIR_NODE_LIST_FOREACH(&node->expr.tuple.values, {
        value_t value = cg_expr(context, current_namespace, scope, node);
        types[i] = value.type;
        llvm_values[i] = value.llvm_value;
    });

    llir_type_t *type = llir_type_cache_get_tuple(context->anon_type_cache, node->expr.tuple.values.count, types);
    LLVMTypeRef llvm_type = llir_type_to_llvm(context, type);

    LLVMValueRef llvm_allocated_tuple = LLVMBuildAlloca(context->llvm_builder, llvm_type, "expr.tuple");
    for(size_t i = 0; i < node->expr.tuple.values.count; i++) {
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
    value_t value_from = cg_expr(context, current_namespace, scope, node->expr.cast.value);
    if(llir_type_eq(node->expr.cast.type, value_from.type)) return value_from;

    llir_type_t *type_to = node->expr.cast.type;
    llir_type_t *type_from = value_from.type;

    if(type_to->kind == LLIR_TYPE_KIND_INTEGER && value_from.type->kind == LLIR_TYPE_KIND_INTEGER) {
        LLVMValueRef value_to;
        if(type_from->integer.bit_size == type_to->integer.bit_size) {
            value_to = value_from.llvm_value;
        } else {
            if(type_from->integer.bit_size > type_to->integer.bit_size) {
                value_to = LLVMBuildTrunc(context->llvm_builder, value_from.llvm_value, llir_type_to_llvm(context, type_to), "cast.trunc");
            } else {
                if(type_to->integer.is_signed) {
                    value_to = LLVMBuildSExt(context->llvm_builder, value_from.llvm_value, llir_type_to_llvm(context, type_to), "cast.sext");
                } else {
                    value_to = LLVMBuildZExt(context->llvm_builder, value_from.llvm_value, llir_type_to_llvm(context, type_to), "cast.zext");
                }
            }
        }
        return (value_t) { .type = type_to, .llvm_value = value_to };
    }

    if(type_from->kind == LLIR_TYPE_KIND_POINTER && type_to->kind == LLIR_TYPE_KIND_POINTER) return (value_t) {
        .type = type_to,
        .llvm_value = value_from.llvm_value
    };

    if(type_from->kind == LLIR_TYPE_KIND_POINTER && type_to->kind == LLIR_TYPE_KIND_INTEGER) return (value_t) {
        .type = type_to,
        .llvm_value = LLVMBuildPtrToInt(context->llvm_builder, value_from.llvm_value, llir_type_to_llvm(context, type_to), "cast.ptrtoint")
    };

    if(type_from->kind == LLIR_TYPE_KIND_INTEGER && type_to->kind == LLIR_TYPE_KIND_POINTER) return (value_t) {
        .type = type_to,
        .llvm_value = LLVMBuildIntToPtr(context->llvm_builder, value_from.llvm_value, llir_type_to_llvm(context, type_to), "cast.inttoptr")
    };

    if(type_from->kind == LLIR_TYPE_KIND_ARRAY && type_to->kind == LLIR_TYPE_KIND_POINTER) {
        if(llir_type_eq(type_from->array.type, type_to->pointer.pointee)) return (value_t) {
            .type = type_to,
            .llvm_value = value_from.llvm_value
        };
    }

    diag_error(node->source_location, "invalid cast");
}

static value_t cg_expr_subscript(CG_EXPR_PARAMS) {
    value_t value = cg_expr_ext(context, current_namespace, scope, node->expr.subscript.value, false);

    switch(node->expr.subscript.type) {
        case LLIR_NODE_SUBSCRIPT_TYPE_INDEX: {
            value_t index = cg_expr(context, current_namespace, scope, node->expr.subscript.index);

            if(index.type->kind != LLIR_TYPE_KIND_INTEGER) diag_error(node->source_location, "invalid type for indexing");

            llir_type_t *type;
            LLVMValueRef llvm_value;
            switch(value.type->kind) {
                case LLIR_TYPE_KIND_ARRAY:
                    type = value.type->array.type;
                    llvm_value = LLVMBuildGEP2(context->llvm_builder, llir_type_to_llvm(context, value.type), value.llvm_value, (LLVMValueRef[]) { LLVMConstInt(LLVMInt64TypeInContext(context->llvm_context), 0, false), index.llvm_value }, 2, "expr.subscript");
                    break;
                case LLIR_TYPE_KIND_POINTER:
                    type = value.type->pointer.pointee;
                    llvm_value = LLVMBuildGEP2(context->llvm_builder, llir_type_to_llvm(context, type), resolve_ref(context, value).llvm_value, &index.llvm_value, 1, "expr.subscript");
                    break;
                default: diag_error(node->source_location, "invalid type for indexing");
            }

            return (value_t) {
                .is_ref = true,
                .type = type,
                .llvm_value = llvm_value
            };
        }
        case LLIR_NODE_SUBSCRIPT_TYPE_INDEX_CONST: {
            llir_type_t *type;
            LLVMValueRef llvm_value;
            switch(value.type->kind) {
                case LLIR_TYPE_KIND_TUPLE:
                    if(node->expr.subscript.index_const >= value.type->tuple.type_count) diag_error(node->source_location, "index out of bounds");
                    type = value.type->tuple.types[node->expr.subscript.index_const];
                    llvm_value = LLVMBuildStructGEP2(context->llvm_builder, llir_type_to_llvm(context, value.type), value.llvm_value, node->expr.subscript.index_const, "expr.subscript");
                    break;
                case LLIR_TYPE_KIND_ARRAY:
                    if(node->expr.subscript.index_const >= value.type->array.size) diag_error(node->source_location, "index out of bounds");
                    type = value.type->array.type;
                    llvm_value = LLVMBuildStructGEP2(context->llvm_builder, llir_type_to_llvm(context, value.type), value.llvm_value, node->expr.subscript.index_const, "expr.subscript");
                    break;
                case LLIR_TYPE_KIND_POINTER:
                    type = value.type->pointer.pointee;
                    llvm_value = LLVMBuildGEP2(context->llvm_builder, llir_type_to_llvm(context, type), resolve_ref(context, value).llvm_value, (LLVMValueRef[]) { LLVMConstInt(LLVMInt64TypeInContext(context->llvm_context), node->expr.subscript.index_const, false) }, 1, "expr.subscript");
                    break;
                default: diag_error(node->source_location, "invalid type for constant indexing");
            }

            return (value_t) {
                .is_ref = true,
                .type = type,
                .llvm_value = llvm_value
            };
        }
        case LLIR_NODE_SUBSCRIPT_TYPE_MEMBER: {
            if(value.type->kind != LLIR_TYPE_KIND_STRUCTURE) diag_error(node->source_location, "not a struct");

            for(size_t i = 0; i < value.type->structure.member_count; i++) {
                llir_type_structure_member_t *member = &value.type->structure.members[i];
                if(strcmp(member->name, node->expr.subscript.member) != 0) continue;
                return (value_t) {
                    .is_ref = true,
                    .type = member->type,
                    .llvm_value = LLVMBuildStructGEP2(context->llvm_builder, llir_type_to_llvm(context, value.type), value.llvm_value, i, "expr.subscript")
                };
            }
            diag_error(node->source_location, "unknown member `%s`", node->expr.subscript.member);
        }
    }
    assert(false);
}

static value_t cg_expr_selector(CG_EXPR_PARAMS) {
    llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->expr.selector.name, LLIR_SYMBOL_KIND_MODULE);
    if(symbol == NULL) {
        symbol = llir_namespace_find_symbol_with_kind(context->root_namespace, node->expr.selector.name, LLIR_SYMBOL_KIND_MODULE);
        if(symbol == NULL) goto enumeration;
    }
    return cg_expr_ext(context, symbol->module.namespace, scope, node->expr.selector.value, false);

    enumeration:
    symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->expr.selector.name, LLIR_SYMBOL_KIND_ENUMERATION);
    if(symbol == NULL) {
        symbol = llir_namespace_find_symbol_with_kind(context->root_namespace, node->expr.selector.name, LLIR_SYMBOL_KIND_ENUMERATION);
        if(symbol == NULL) diag_error(node->source_location, "unknown selector `%s`", node->expr.selector.name);
    }
    if(node->expr.selector.value->type != LLIR_NODE_TYPE_EXPR_VARIABLE) diag_error(node->source_location, "invalid operation on enum");
    for(size_t i = 0; i < symbol->enumeration.type->enumeration.member_count; i++) {
        if(strcmp(node->expr.selector.value->expr.variable.name, symbol->enumeration.members[i]) != 0) continue;
        return (value_t) {
            .type = symbol->enumeration.type,
            .llvm_value = LLVMConstInt(llir_type_to_llvm(context, symbol->enumeration.type), i, false)
        };
    }
    diag_error(node->source_location, "unknown enum member");
}

// Common

static void cg_root(CG_TLC_PARAMS) {
    assert(node->type == LLIR_NODE_TYPE_ROOT);
    LLIR_NODE_LIST_FOREACH(&node->root.tlcs, cg_tlc(context, current_namespace, node));
}

static void cg_tlc(CG_TLC_PARAMS) {
    switch(node->type) {
        case LLIR_NODE_TYPE_TLC_MODULE: cg_tlc_module(context, current_namespace, node); break;
        case LLIR_NODE_TYPE_TLC_FUNCTION: cg_tlc_function(context, current_namespace, node); break;

        case LLIR_NODE_TYPE_ROOT: assert(false);
        LLIR_CASE_STMT(assert(false));
        LLIR_CASE_EXPRESSION(assert(false));
    }
}

static void cg_stmt(CG_STMT_PARAMS) {
    switch(node->type) {
        case LLIR_NODE_TYPE_STMT_BLOCK: cg_stmt_block(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_STMT_DECLARATION: cg_stmt_declaration(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_STMT_EXPRESSION: cg_expr(context, current_namespace, scope, node->stmt_expression.expression); break;
        case LLIR_NODE_TYPE_STMT_RETURN: cg_stmt_return(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_STMT_IF: cg_stmt_if(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_STMT_WHILE: cg_stmt_while(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_STMT_CONTINUE: cg_stmt_continue(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_STMT_BREAK: cg_stmt_break(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_STMT_FOR: cg_stmt_for(context, current_namespace, scope, node); break;

        case LLIR_NODE_TYPE_ROOT: assert(false);
        LLIR_CASE_TLC(assert(false));
        LLIR_CASE_EXPRESSION(assert(false));
    }
}

static value_t cg_expr(CG_EXPR_PARAMS) {
    return cg_expr_ext(context, current_namespace, scope, node, true);
}

static value_t cg_expr_ext(CG_EXPR_PARAMS, bool do_resolve_ref) {
    value_t value;
    switch(node->type) {
        case LLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC: value = cg_expr_literal_numeric(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_LITERAL_STRING: value = cg_expr_literal_string(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_LITERAL_CHAR: value = cg_expr_literal_char(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_LITERAL_BOOL: value = cg_expr_literal_bool(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_BINARY: value = cg_expr_binary(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_UNARY: value = cg_expr_unary(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_VARIABLE: value = cg_expr_variable(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_CALL: value = cg_expr_call(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_TUPLE: value = cg_expr_tuple(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_CAST: value = cg_expr_cast(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_SUBSCRIPT: value = cg_expr_subscript(context, current_namespace, scope, node); break;
        case LLIR_NODE_TYPE_EXPR_SELECTOR: value = cg_expr_selector(context, current_namespace, scope, node); break;

        case LLIR_NODE_TYPE_ROOT: assert(false);
        LLIR_CASE_TLC(assert(false));
        LLIR_CASE_STMT(assert(false));
    }
    if(do_resolve_ref) return resolve_ref(context, value);
    return value;
}

// //
//
//

static void populate_namespace(context_t *context, llir_namespace_t *namespace) {
    for(size_t i = 0; i < namespace->symbol_count; i++) {
        llir_symbol_t *symbol = namespace->symbols[i];
        switch(symbol->kind) {
            case LLIR_SYMBOL_KIND_MODULE: populate_namespace(context, symbol->module.namespace); break;
            case LLIR_SYMBOL_KIND_FUNCTION:
                const char *link_name = symbol->function.link_name;
                if(link_name == NULL) link_name = mangle_name(symbol->name, symbol->namespace->parent);
                symbol->function.codegen_data = LLVMAddFunction(context->llvm_module, link_name, llir_type_function_to_llvm(context, symbol->function.function_type));
                break;
            case LLIR_SYMBOL_KIND_VARIABLE:
                LLVMTypeRef type = llir_type_to_llvm(context, symbol->variable.type);
                symbol->variable.codegen_data = LLVMAddGlobal(context->llvm_module, type, mangle_name(symbol->name, symbol->namespace->parent));
                LLVMSetInitializer(symbol->variable.codegen_data, LLVMConstNull(type));
                break;
            case LLIR_SYMBOL_KIND_ENUMERATION: break;
        }
    }
}

void codegen(llir_node_t *root_node, llir_namespace_t *root_namespace, llir_type_cache_t *anon_type_cache, const char *path, const char *passes, LLVMCodeModel code_model) {
    assert(root_node->type == LLIR_NODE_TYPE_ROOT);

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

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, code_model);
    if(machine == NULL) log_fatal("failed to create target machine");

    context_t context;
    context.llvm_context = LLVMContextCreate();
    context.llvm_builder = LLVMCreateBuilderInContext(context.llvm_context);
    context.llvm_module = LLVMModuleCreateWithNameInContext("CharonModule", context.llvm_context);
    context.root_namespace = root_namespace;
    context.anon_type_cache = anon_type_cache;

    populate_namespace(&context, root_namespace);
    cg_root(&context, context.root_namespace, root_node);

    error_message = NULL;
    LLVMBool failure = LLVMVerifyModule(context.llvm_module, LLVMReturnStatusAction, &error_message);
    if(failure) log_fatal("failed to verify module (%s)", error_message);
    LLVMDisposeMessage(error_message);

    if(passes != NULL) {
        LLVMErrorRef error = LLVMRunPasses(context.llvm_module, passes, NULL, LLVMCreatePassBuilderOptions());
        if(error != NULL) log_warning("llvm ir optimization failed (%s)", LLVMGetErrorMessage(error));
    }

    LLVMSetTarget(context.llvm_module, triple);
    char *layout = LLVMCopyStringRepOfTargetData(LLVMCreateTargetDataLayout(machine));
    LLVMSetDataLayout(context.llvm_module, layout);
    LLVMDisposeMessage(layout);

    if(LLVMTargetMachineEmitToFile(machine, context.llvm_module, path, LLVMObjectFile, &error_message) != 0) log_fatal("emit failed (%s)", error_message);

    LLVMDisposeBuilder(context.llvm_builder);
    LLVMDisposeModule(context.llvm_module);
    LLVMContextDispose(context.llvm_context);
}

void codegen_ir(llir_node_t *root_node, llir_namespace_t *root_namespace, llir_type_cache_t *anon_type_cache, const char *path) {
    context_t context;
    context.llvm_context = LLVMContextCreate();
    context.llvm_builder = LLVMCreateBuilderInContext(context.llvm_context);
    context.llvm_module = LLVMModuleCreateWithNameInContext("CharonModule", context.llvm_context);
    context.root_namespace = root_namespace;
    context.anon_type_cache = anon_type_cache;

    populate_namespace(&context, root_namespace);
    cg_root(&context, context.root_namespace, root_node);

    LLVMBool failure;
    char *error_message;

    failure = LLVMVerifyModule(context.llvm_module, LLVMReturnStatusAction, &error_message);
    if(failure) log_fatal("failed to verify module (%s)", error_message);
    LLVMDisposeMessage(error_message);

    failure = LLVMPrintModuleToFile(context.llvm_module, path, &error_message);
    if(failure) {
        log_fatal("emit failed (%s)\n", error_message);
        LLVMDisposeMessage(error_message);
    }

    LLVMDisposeBuilder(context.llvm_builder);
    LLVMDisposeModule(context.llvm_module);
    LLVMContextDispose(context.llvm_context);
}