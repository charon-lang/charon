#include "codegen.h"
#include "ir/ir.h"
#include "lib/alloc.h"
#include "lib/diag.h"
#include "lib/log.h"

#include "llvm-c/Types.h"

#include <assert.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <stddef.h>
#include <string.h>

#define TYPE_BOOL ir_type_cache_get_integer(context->type_cache, 1, false, false)

struct codegen_context {
    LLVMContextRef llvm_context;
    LLVMBuilderRef llvm_builder;
    LLVMModuleRef llvm_module;
    LLVMTargetMachineRef llvm_machine;
};

typedef struct {
    LLVMContextRef llvm_context;
    LLVMBuilderRef llvm_builder;
    LLVMModuleRef llvm_module;

    ir_unit_t *unit;
    ir_type_cache_t *type_cache;

    struct {
        ir_type_t *type;
        bool has_returned;
        bool wont_return;
    } return_state;

    struct {
        bool in_loop, has_terminated, can_break;
        LLVMBasicBlockRef bb_beginning, bb_end;
    } loop_state;
} context_t;

typedef struct {
    bool is_ref;
    LLVMValueRef llvm_value;
} value_t;

static char *mangle_name(const char *name, ir_module_t *module) {
    char *mangled_name = alloc_strdup(name);
    while(module != NULL) {
        size_t name_length = strlen(mangled_name);
        size_t module_name_length = strlen(module->name);
        char *new = alloc(module_name_length + 2 + name_length + 1);
        memcpy(new, module->name, module_name_length);
        memset(&new[module_name_length], ':', 2);
        memcpy(&new[module_name_length + 2], mangled_name, name_length);
        new[module_name_length + 2 + name_length] = '\0';

        alloc_free(mangled_name);
        mangled_name = new;
        module = module->parent;
    }
    return mangled_name;
}

static LLVMTypeRef ir_type_to_llvm(context_t *context, ir_type_t *type) {
    assert(type != NULL);
    switch(type->kind) {
        case IR_TYPE_KIND_VOID: return LLVMVoidTypeInContext(context->llvm_context);
        case IR_TYPE_KIND_INTEGER:
            switch(type->integer.bit_size) {
                case 1:  return LLVMInt1TypeInContext(context->llvm_context);
                case 8:  return LLVMInt8TypeInContext(context->llvm_context);
                case 16: return LLVMInt16TypeInContext(context->llvm_context);
                case 32: return LLVMInt32TypeInContext(context->llvm_context);
                case 64: return LLVMInt64TypeInContext(context->llvm_context);
                default: assert(false);
            }
        case IR_TYPE_KIND_POINTER: return LLVMPointerTypeInContext(context->llvm_context, 0);
        case IR_TYPE_KIND_ARRAY:   return LLVMArrayType2(ir_type_to_llvm(context, type->array.type), type->array.size);
        case IR_TYPE_KIND_TUPLE:   {
            LLVMTypeRef types[type->tuple.type_count];
            for(size_t i = 0; i < type->tuple.type_count; i++) types[i] = ir_type_to_llvm(context, type->tuple.types[i]);
            return LLVMStructTypeInContext(context->llvm_context, types, type->tuple.type_count, false);
        }
        case IR_TYPE_KIND_STRUCTURE: {
            LLVMTypeRef types[type->structure.member_count];
            for(size_t i = 0; i < type->structure.member_count; i++) types[i] = ir_type_to_llvm(context, type->structure.members[i].type);
            return LLVMStructTypeInContext(context->llvm_context, types, type->structure.member_count, type->structure.packed);
        }
        case IR_TYPE_KIND_FUNCTION_REFERENCE: return LLVMPointerTypeInContext(context->llvm_context, 0);
        case IR_TYPE_KIND_ENUMERATION:        return LLVMIntTypeInContext(context->llvm_context, type->enumeration.bit_size);
    }
    assert(false);
}

static LLVMTypeRef ir_function_prototype_to_llvm(context_t *context, ir_function_prototype_t prototype) {
    LLVMTypeRef fn_return_type = ir_type_to_llvm(context, prototype.return_type);
    if(prototype.argument_count > 0) {
        LLVMTypeRef fn_arguments[prototype.argument_count];
        for(size_t i = 0; i < prototype.argument_count; i++) {
            fn_arguments[i] = ir_type_to_llvm(context, prototype.arguments[i]);
        }
        return LLVMFunctionType(fn_return_type, fn_arguments, prototype.argument_count, prototype.varargs);
    }
    return LLVMFunctionType(fn_return_type, NULL, 0, prototype.varargs);
}

static LLVMValueRef resolve_ref(context_t *context, ir_type_t *type, value_t value) {
    if(!value.is_ref) return value.llvm_value;
    assert(value.llvm_value != NULL);
    return LLVMBuildLoad2(context->llvm_builder, ir_type_to_llvm(context, type), value.llvm_value, "resolve_ref");
}

// //
// Codegen
//

#define CG_STMT_PARAMS context_t *context, ir_stmt_t *stmt
#define CG_EXPR_PARAMS context_t *context, ir_expr_t *expr

static void cg_stmt(CG_STMT_PARAMS);
static LLVMValueRef cg_expr(CG_EXPR_PARAMS);
static value_t cg_expr_ext(CG_EXPR_PARAMS);

// Statements

static void cg_stmt_block(CG_STMT_PARAMS) {
    VECTOR_FOREACH(&stmt->block.statements, i, elem) {
        cg_stmt(context, elem);
        if((context->return_state.has_returned || context->return_state.wont_return) || (context->loop_state.in_loop && context->loop_state.has_terminated)) break;
    }
}

static void cg_stmt_declaration(CG_STMT_PARAMS) {
    ir_variable_t *variable = stmt->declaration.variable;

    LLVMValueRef func_parent = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));

    LLVMValueRef llvm_value = NULL;
    ir_type_t *type = variable->type;
    if(variable->initial_value != NULL) llvm_value = cg_expr(context, variable->initial_value);

    LLVMBasicBlockRef bb_entry = LLVMGetEntryBasicBlock(func_parent);
    LLVMBuilderRef entry_builder = LLVMCreateBuilderInContext(context->llvm_context);
    LLVMPositionBuilder(entry_builder, bb_entry, LLVMGetFirstInstruction(bb_entry));
    LLVMValueRef ref = LLVMBuildAlloca(entry_builder, ir_type_to_llvm(context, type), variable->name);
    LLVMDisposeBuilder(entry_builder);
    if(llvm_value != NULL) LLVMBuildStore(context->llvm_builder, llvm_value, ref);
    variable->codegen_data = ref;
}

static void cg_stmt_return(CG_STMT_PARAMS) {
    context->return_state.has_returned = true;
    if(context->return_state.type->kind == IR_TYPE_KIND_VOID) {
        if(stmt->_return.value != NULL) {
            if(stmt->_return.value->type != IR_TYPE_KIND_VOID) diag_error(stmt->source_location, LANG_E_UNEXPECTED_RETURN);
            cg_expr(context, stmt->_return.value);
        }
        LLVMBuildRetVoid(context->llvm_builder);
        return;
    }
    if(stmt->_return.value == NULL) diag_error(stmt->source_location, LANG_E_MISSING_RETURN);
    LLVMBuildRet(context->llvm_builder, cg_expr(context, stmt->_return.value));
}

static void cg_stmt_if(CG_STMT_PARAMS) {
    assert(ir_type_eq(stmt->_if.condition->type, TYPE_BOOL));

    LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));
    LLVMBasicBlockRef bb_body = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "if.body");
    LLVMBasicBlockRef bb_else_body = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "if.else_body");
    LLVMBasicBlockRef bb_end = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "if.end");

    LLVMBuildCondBr(context->llvm_builder, cg_expr(context, stmt->_if.condition), bb_body, stmt->_if.else_body != NULL ? bb_else_body : bb_end);

    // Create then, aka body
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_body);
    cg_stmt(context, stmt->_if.body);
    bool then_wont_return = context->return_state.wont_return;
    bool then_returned = context->return_state.has_returned;
    bool then_terminated = context->loop_state.in_loop && context->loop_state.has_terminated;
    if(!then_wont_return && !then_returned && !then_terminated) LLVMBuildBr(context->llvm_builder, bb_end);

    // Create else body
    bool else_wont_return = false, else_returned = false, else_terminated = false;
    if(stmt->_if.else_body != NULL) {
        context->return_state.has_returned = false;
        context->loop_state.has_terminated = false;

        LLVMPositionBuilderAtEnd(context->llvm_builder, bb_else_body);
        cg_stmt(context, stmt->_if.else_body);
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
    assert(stmt->_while.condition == NULL || ir_type_eq(stmt->_while.condition->type, TYPE_BOOL));

    LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));
    LLVMBasicBlockRef bb_top = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "while.cond");
    LLVMBasicBlockRef bb_body = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "while.body");
    LLVMBasicBlockRef bb_end = LLVMCreateBasicBlockInContext(context->llvm_context, "while.end");

    bool create_end = stmt->_while.condition != NULL;

    LLVMBuildBr(context->llvm_builder, bb_top);

    // Condition
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_top);
    if(stmt->_while.condition != NULL) {
        LLVMBuildCondBr(context->llvm_builder, cg_expr(context, stmt->_while.condition), bb_body, bb_end);
    } else {
        LLVMBuildBr(context->llvm_builder, bb_body);
    }

    // Body
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_body);

    typeof(context->loop_state) prev = context->loop_state;
    context->loop_state.in_loop = true;
    context->loop_state.has_terminated = false;
    context->loop_state.can_break = false;
    context->loop_state.bb_beginning = bb_top;
    context->loop_state.bb_end = bb_end;

    if(stmt->_while.body != NULL) cg_stmt(context, stmt->_while.body);
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
    if(!context->loop_state.in_loop) diag_error(stmt->source_location, LANG_E_NOT_IN_LOOP);
    context->loop_state.has_terminated = true;
    LLVMBuildBr(context->llvm_builder, context->loop_state.bb_beginning);
}

static void cg_stmt_break(CG_STMT_PARAMS) {
    if(!context->loop_state.in_loop) diag_error(stmt->source_location, LANG_E_NOT_IN_LOOP);
    context->loop_state.has_terminated = true;
    context->loop_state.can_break = true;
    LLVMBuildBr(context->llvm_builder, context->loop_state.bb_end);
}

static void cg_stmt_for(CG_STMT_PARAMS) {
    assert(stmt->_for.condition == NULL || ir_type_eq(stmt->_for.condition->type, TYPE_BOOL));

    LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));
    LLVMBasicBlockRef bb_top = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "for.cond");
    LLVMBasicBlockRef bb_body = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "for.body");
    LLVMBasicBlockRef bb_after = LLVMCreateBasicBlockInContext(context->llvm_context, "for.after");
    LLVMBasicBlockRef bb_end = LLVMCreateBasicBlockInContext(context->llvm_context, "for.end");

    bool create_end = stmt->_for.condition != NULL;

    if(stmt->_for.declaration != NULL) cg_stmt_declaration(context, stmt->_for.declaration);

    LLVMBuildBr(context->llvm_builder, bb_top);

    // Condition
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_top);
    if(stmt->_for.condition != NULL) {
        LLVMBuildCondBr(context->llvm_builder, cg_expr(context, stmt->_for.condition), bb_body, bb_end);
    } else {
        LLVMBuildBr(context->llvm_builder, bb_body);
    }

    // Body
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_body);

    typeof(context->loop_state) prev = context->loop_state;
    context->loop_state.in_loop = true;
    context->loop_state.has_terminated = false;
    context->loop_state.can_break = false;
    context->loop_state.bb_beginning = stmt->_for.expr_after != NULL ? bb_after : bb_top;
    context->loop_state.bb_end = bb_end;

    if(stmt->_for.body != NULL) cg_stmt(context, stmt->_for.body);

    if(stmt->_for.expr_after != NULL) {
        LLVMBuildBr(context->llvm_builder, bb_after);

        LLVMAppendExistingBasicBlock(llvm_func, bb_after);
        LLVMPositionBuilderAtEnd(context->llvm_builder, bb_after);

        cg_expr(context, stmt->_for.expr_after);
    }

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

static void cg_stmt_switch(CG_STMT_PARAMS) {
    LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));

    LLVMBasicBlockRef bb_end = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "switch.end");
    LLVMBasicBlockRef bb_else = stmt->_switch.default_body == NULL ? bb_end : LLVMInsertBasicBlock(bb_end, "switch.default");

    LLVMValueRef switch_value = LLVMBuildSwitch(context->llvm_builder, cg_expr(context, stmt->_switch.value), bb_else, stmt->_switch.case_count);

    if(stmt->_switch.default_body != NULL) {
        LLVMPositionBuilderAtEnd(context->llvm_builder, bb_else);

        cg_stmt(context, stmt->_switch.default_body);
        LLVMBuildBr(context->llvm_builder, bb_end);
    }

    union {
        unsigned long long u;
        long long s;
    } cached_values[stmt->_switch.case_count];

    for(size_t i = 0; i < stmt->_switch.case_count; i++) {
        LLVMBasicBlockRef bb_case = LLVMInsertBasicBlock(bb_end, "switch.case");

        LLVMValueRef value = cg_expr(context, stmt->_switch.cases[i].value);
        assert(stmt->_switch.value->type->kind == IR_TYPE_KIND_INTEGER);
        if(stmt->_switch.value->type->integer.is_signed) {
            cached_values[i].s = LLVMConstIntGetSExtValue(value);
        } else {
            cached_values[i].u = LLVMConstIntGetZExtValue(value);
        }

        for(size_t j = 0; j < i; j++) {
            if(cached_values[i].u != cached_values[j].u) continue;
            diag_error(stmt->_switch.cases[i].value->source_location, LANG_E_DUPLICATE_CASE);
        }

        LLVMAddCase(switch_value, value, bb_case);

        LLVMPositionBuilderAtEnd(context->llvm_builder, bb_case);

        cg_stmt(context, stmt->_switch.cases[i].body);
        LLVMBuildBr(context->llvm_builder, bb_end);
    }

    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_end);
}

// Expressions

static value_t cg_expr_literal_numeric(CG_EXPR_PARAMS) {
    assert(expr->type->kind == IR_TYPE_KIND_INTEGER);
    return (value_t) { .llvm_value = LLVMConstInt(ir_type_to_llvm(context, expr->type), expr->literal.numeric_value, expr->type->integer.is_signed) };
}

static value_t cg_expr_literal_char(CG_EXPR_PARAMS) {
    assert(expr->type->kind == IR_TYPE_KIND_INTEGER);
    return (value_t) { .llvm_value = LLVMConstInt(ir_type_to_llvm(context, expr->type), expr->literal.char_value, expr->type->integer.is_signed) };
}

static value_t cg_expr_literal_string(CG_EXPR_PARAMS) {
    assert(expr->type->kind == IR_TYPE_KIND_ARRAY);
    LLVMValueRef value = LLVMAddGlobal(context->llvm_module, ir_type_to_llvm(context, expr->type), "expr.literal_string");
    LLVMSetLinkage(value, LLVMInternalLinkage);
    LLVMSetGlobalConstant(value, 1);
    LLVMSetInitializer(value, LLVMConstStringInContext(context->llvm_context, expr->literal.string_value, expr->type->array.size, true));
    return (value_t) { .is_ref = true, .llvm_value = value };
}

static value_t cg_expr_literal_bool(CG_EXPR_PARAMS) {
    assert(ir_type_eq(expr->type, TYPE_BOOL));
    return (value_t) { .llvm_value = LLVMConstInt(ir_type_to_llvm(context, expr->type), expr->literal.bool_value, expr->type->integer.is_signed) };
}

static value_t cg_expr_literal_struct(CG_EXPR_PARAMS) {
    assert(expr->type->kind == IR_TYPE_KIND_STRUCTURE);

    size_t member_count = expr->type->structure.member_count;
    LLVMValueRef members[member_count];
    for(size_t i = 0; i < member_count; i++) {
        ir_literal_struct_member_t *member = expr->literal.struct_value.members[i];
        if(member == NULL) {
            members[i] = LLVMConstNull(ir_type_to_llvm(context, expr->type->structure.members[i].type));
            continue;
        }
        members[i] = cg_expr(context, expr->literal.struct_value.members[i]->value);
    }

    if(expr->is_const) return (value_t) { .llvm_value = LLVMConstStructInContext(context->llvm_context, members, member_count, expr->type->structure.packed) };

    LLVMValueRef value = LLVMGetUndef(ir_type_to_llvm(context, expr->type));
    for(size_t i = 0; i < member_count; i++) {
        value = LLVMBuildInsertValue(context->llvm_builder, value, members[i], i, expr->type->structure.members[i].name);
    }
    return (value_t) { .llvm_value = value };
}

static value_t cg_expr_binary(CG_EXPR_PARAMS) {
    switch(expr->binary.operation) {
        enum {
            LOGICAL_AND,
            LOGICAL_OR
        } op;
        case IR_BINARY_OPERATION_LOGICAL_AND: op = LOGICAL_AND; goto logical;
        case IR_BINARY_OPERATION_LOGICAL_OR:
            op = LOGICAL_OR;
            goto logical;
        logical: {
            assert(ir_type_eq(expr->type, TYPE_BOOL));
            assert(ir_type_eq(expr->binary.left->type, TYPE_BOOL));
            assert(ir_type_eq(expr->binary.right->type, TYPE_BOOL));

            LLVMValueRef llvm_func = LLVMGetBasicBlockParent(LLVMGetInsertBlock(context->llvm_builder));
            LLVMBasicBlockRef bb_phi = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "expr.binary.logical.phi");
            LLVMBasicBlockRef bb_end = LLVMAppendBasicBlockInContext(context->llvm_context, llvm_func, "expr.binary.logical.end");

            LLVMBasicBlockRef blocks[2];

            // Left
            LLVMValueRef left = cg_expr(context, expr->binary.left);
            LLVMBuildCondBr(context->llvm_builder, left, op == LOGICAL_OR ? bb_phi : bb_end, op == LOGICAL_OR ? bb_end : bb_phi);
            blocks[0] = LLVMGetInsertBlock(context->llvm_builder);

            // Right
            LLVMPositionBuilderAtEnd(context->llvm_builder, bb_end);
            LLVMValueRef right = cg_expr(context, expr->binary.right);
            LLVMBuildBr(context->llvm_builder, bb_phi);
            blocks[1] = LLVMGetInsertBlock(context->llvm_builder);

            // Phi
            LLVMPositionBuilderAtEnd(context->llvm_builder, bb_phi);
            LLVMValueRef phi = LLVMBuildPhi(context->llvm_builder, ir_type_to_llvm(context, expr->type), "expr.binary.logical.value");
            LLVMAddIncoming(phi, (LLVMValueRef[]) { left, right }, blocks, 2);

            return (value_t) { .llvm_value = phi };
        }
        default: break;
    }

    LLVMValueRef right = cg_expr(context, expr->binary.right);
    value_t left_value = cg_expr_ext(context, expr->binary.left);

    if(expr->binary.operation == IR_BINARY_OPERATION_ASSIGN) {
        if(!left_value.is_ref) diag_error(expr->source_location, LANG_E_INVALID_LVALUE);
        LLVMBuildStore(context->llvm_builder, right, left_value.llvm_value);
        return (value_t) { .llvm_value = right };
    }
    LLVMValueRef left = resolve_ref(context, expr->binary.left->type, left_value);

    switch(expr->binary.operation) {
        case IR_BINARY_OPERATION_EQUAL:     return (value_t) { .llvm_value = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, left, right, "expr.binary.eq") };
        case IR_BINARY_OPERATION_NOT_EQUAL: return (value_t) { .llvm_value = LLVMBuildICmp(context->llvm_builder, LLVMIntNE, left, right, "expr.binary.ne") };
        default:                            break;
    }

    ir_type_t *type = expr->binary.left->type;
    assert(type->kind == IR_TYPE_KIND_INTEGER);
    switch(expr->binary.operation) {
        case IR_BINARY_OPERATION_ADDITION:       return (value_t) { .llvm_value = LLVMBuildAdd(context->llvm_builder, left, right, "expr.binary.add") };
        case IR_BINARY_OPERATION_SUBTRACTION:    return (value_t) { .llvm_value = LLVMBuildSub(context->llvm_builder, left, right, "expr.binary.sub") };
        case IR_BINARY_OPERATION_MULTIPLICATION: return (value_t) { .llvm_value = LLVMBuildMul(context->llvm_builder, left, right, "expr.binary.mul") };
        case IR_BINARY_OPERATION_DIVISION:       return (value_t) { .llvm_value = type->integer.is_signed ? LLVMBuildSDiv(context->llvm_builder, left, right, "expr.binary.sdiv") : LLVMBuildUDiv(context->llvm_builder, left, right, "expr.binary.udiv") };
        case IR_BINARY_OPERATION_MODULO:         return (value_t) { .llvm_value = type->integer.is_signed ? LLVMBuildSRem(context->llvm_builder, left, right, "expr.binary.srem") : LLVMBuildURem(context->llvm_builder, left, right, "expr.binary.urem") };
        case IR_BINARY_OPERATION_GREATER:        return (value_t) { .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSGT : LLVMIntUGT, left, right, "expr.binary.gt") };
        case IR_BINARY_OPERATION_GREATER_EQUAL:  return (value_t) { .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSGE : LLVMIntUGE, left, right, "expr.binary.ge") };
        case IR_BINARY_OPERATION_LESS:           return (value_t) { .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSLT : LLVMIntULT, left, right, "expr.binary.lt") };
        case IR_BINARY_OPERATION_LESS_EQUAL:     return (value_t) { .llvm_value = LLVMBuildICmp(context->llvm_builder, type->integer.is_signed ? LLVMIntSLE : LLVMIntULE, left, right, "expr.binary.le") };
        case IR_BINARY_OPERATION_SHIFT_LEFT:     return (value_t) { .llvm_value = LLVMBuildShl(context->llvm_builder, left, right, "expr.binary.shl") };
        case IR_BINARY_OPERATION_SHIFT_RIGHT:    return (value_t) { .llvm_value = type->integer.is_signed ? LLVMBuildAShr(context->llvm_builder, left, right, "expr.binary.ashr") : LLVMBuildLShr(context->llvm_builder, left, right, "expr.binary.lshr") };
        case IR_BINARY_OPERATION_AND:            return (value_t) { .llvm_value = LLVMBuildAnd(context->llvm_builder, left, right, "expr.binary.and") };
        case IR_BINARY_OPERATION_OR:             return (value_t) { .llvm_value = LLVMBuildOr(context->llvm_builder, left, right, "expr.binary.or") };
        case IR_BINARY_OPERATION_XOR:            return (value_t) { .llvm_value = LLVMBuildXor(context->llvm_builder, left, right, "expr.binary.xor") };
        default:                                 break;
    }
    assert(false);
}

static value_t cg_expr_unary(CG_EXPR_PARAMS) {
    value_t operand_value = cg_expr_ext(context, expr->unary.operand);
    if(expr->unary.operation == IR_UNARY_OPERATION_REF) {
        if(!operand_value.is_ref) diag_error(expr->source_location, LANG_E_REF_NOT_VAR);
        return (value_t) { .llvm_value = operand_value.llvm_value };
    }

    LLVMValueRef operand = resolve_ref(context, expr->unary.operand->type, operand_value);
    switch(expr->unary.operation) {
        case IR_UNARY_OPERATION_DEREF:    return (value_t) { .is_ref = true, .llvm_value = operand };
        case IR_UNARY_OPERATION_NOT:      return (value_t) { .llvm_value = LLVMBuildICmp(context->llvm_builder, LLVMIntEQ, operand, LLVMConstInt(ir_type_to_llvm(context, expr->unary.operand->type), 0, false), "") };
        case IR_UNARY_OPERATION_NEGATIVE: return (value_t) { .llvm_value = LLVMBuildNeg(context->llvm_builder, operand, "") };
        default:                          assert(false);
    }
}

static value_t cg_expr_variable(CG_EXPR_PARAMS) {
    return (value_t) { .is_ref = !expr->variable.is_function, .llvm_value = expr->variable.is_function ? expr->variable.function->codegen_data : expr->variable.variable->codegen_data };
}

static value_t cg_expr_call(CG_EXPR_PARAMS) {
    LLVMValueRef value = cg_expr(context, expr->call.function_reference);

    assert(expr->call.function_reference->type->kind == IR_TYPE_KIND_FUNCTION_REFERENCE);
    ir_function_prototype_t prototype = expr->call.function_reference->type->function_reference.prototype;

    LLVMValueRef llvm_value;
    if(VECTOR_SIZE(&expr->call.arguments) > 0) {
        LLVMValueRef arguments[VECTOR_SIZE(&expr->call.arguments)];
        VECTOR_FOREACH(&expr->call.arguments, i, elem) {
            arguments[i] = cg_expr(context, elem);
        }
        llvm_value = LLVMBuildCall2(context->llvm_builder, ir_function_prototype_to_llvm(context, prototype), value, arguments, VECTOR_SIZE(&expr->call.arguments), "");
    } else {
        llvm_value = LLVMBuildCall2(context->llvm_builder, ir_function_prototype_to_llvm(context, prototype), value, NULL, 0, "");
    }

    return (value_t) { .llvm_value = llvm_value };
}

static value_t cg_expr_tuple(CG_EXPR_PARAMS) {
    LLVMValueRef values[VECTOR_SIZE(&expr->tuple.values)];
    VECTOR_FOREACH(&expr->tuple.values, i, elem) values[i] = cg_expr(context, elem);

    LLVMTypeRef llvm_type = ir_type_to_llvm(context, expr->type);
    LLVMValueRef llvm_allocated_tuple = LLVMBuildAlloca(context->llvm_builder, llvm_type, "expr.tuple");
    for(size_t i = 0; i < VECTOR_SIZE(&expr->tuple.values); i++) {
        LLVMValueRef member_ptr = LLVMBuildStructGEP2(context->llvm_builder, llvm_type, llvm_allocated_tuple, i, "");
        LLVMBuildStore(context->llvm_builder, values[i], member_ptr);
    }

    return (value_t) { .is_ref = true, .llvm_value = llvm_allocated_tuple };
}

static value_t cg_expr_cast(CG_EXPR_PARAMS) {
    ir_type_t *type_to = expr->cast.type;
    ir_type_t *type_from = expr->cast.value->type;

    value_t value_from_ext = cg_expr_ext(context, expr->cast.value);

    if(ir_type_eq(type_to, type_from)) return value_from_ext;

    if(type_from->kind == IR_TYPE_KIND_ARRAY && type_to->kind == IR_TYPE_KIND_POINTER && ir_type_eq(type_from->array.type, type_to->pointer.pointee)) {
        assert(value_from_ext.is_ref);
        return (value_t) { .llvm_value = LLVMBuildGEP2(
                               context->llvm_builder,
                               ir_type_to_llvm(context, type_from),
                               value_from_ext.llvm_value,
                               (LLVMValueRef[]) { LLVMConstInt(LLVMInt64TypeInContext(context->llvm_context), 0, false), LLVMConstInt(LLVMInt64TypeInContext(context->llvm_context), 0, false) },
                               2,
                               "cast.arraydecay"
                           ) };
    }

    LLVMValueRef value_from = resolve_ref(context, expr->cast.value->type, value_from_ext);

    if(type_to->kind == IR_TYPE_KIND_INTEGER && type_from->kind == IR_TYPE_KIND_INTEGER) {
        LLVMValueRef value_to;
        if(type_from->integer.bit_size == type_to->integer.bit_size) {
            value_to = value_from;
        } else {
            if(type_from->integer.bit_size > type_to->integer.bit_size) {
                value_to = LLVMBuildTrunc(context->llvm_builder, value_from, ir_type_to_llvm(context, type_to), "cast.trunc");
            } else {
                if(type_to->integer.is_signed) {
                    value_to = LLVMBuildSExt(context->llvm_builder, value_from, ir_type_to_llvm(context, type_to), "cast.sext");
                } else {
                    value_to = LLVMBuildZExt(context->llvm_builder, value_from, ir_type_to_llvm(context, type_to), "cast.zext");
                }
            }
        }
        return (value_t) { .llvm_value = value_to };
    }

    if(type_from->kind == IR_TYPE_KIND_POINTER && type_to->kind == IR_TYPE_KIND_POINTER) return (value_t) { .llvm_value = value_from };

    if(type_from->kind == IR_TYPE_KIND_POINTER && type_to->kind == IR_TYPE_KIND_INTEGER) return (value_t) { .llvm_value = LLVMBuildPtrToInt(context->llvm_builder, value_from, ir_type_to_llvm(context, type_to), "cast.ptrtoint") };

    if(type_from->kind == IR_TYPE_KIND_INTEGER && type_to->kind == IR_TYPE_KIND_POINTER) return (value_t) { .llvm_value = LLVMBuildIntToPtr(context->llvm_builder, value_from, ir_type_to_llvm(context, type_to), "cast.inttoptr") };

    if(type_from->kind == IR_TYPE_KIND_ENUMERATION && type_to->kind == IR_TYPE_KIND_INTEGER) return (value_t) { .llvm_value = value_from };

    diag_error(expr->source_location, LANG_E_INVALID_CAST);
}

static value_t cg_expr_subscript(CG_EXPR_PARAMS) {
    value_t value = cg_expr_ext(context, expr->subscript.value);

    LLVMValueRef llvm_value;
    switch(expr->subscript.kind) {
        case IR_SUBSCRIPT_KIND_INDEX: {
            LLVMValueRef index = cg_expr(context, expr->subscript.index);

            switch(expr->subscript.value->type->kind) {
                case IR_TYPE_KIND_ARRAY:
                    llvm_value = LLVMBuildGEP2(
                        context->llvm_builder,
                        ir_type_to_llvm(context, expr->subscript.value->type),
                        value.llvm_value,
                        (LLVMValueRef[]) { LLVMConstInt(LLVMInt64TypeInContext(context->llvm_context), 0, false), index },
                        2,
                        "expr.subscript"
                    );
                    break;
                case IR_TYPE_KIND_POINTER: llvm_value = LLVMBuildGEP2(context->llvm_builder, ir_type_to_llvm(context, expr->type), resolve_ref(context, expr->subscript.value->type, value), &index, 1, "expr.subscript"); break;
                default:                   diag_error(expr->source_location, LANG_E_INVALID_TYPE);
            }
            break;
        }
        case IR_SUBSCRIPT_KIND_INDEX_CONST: {
            switch(expr->subscript.value->type->kind) {
                case IR_TYPE_KIND_TUPLE: llvm_value = LLVMBuildStructGEP2(context->llvm_builder, ir_type_to_llvm(context, expr->subscript.value->type), value.llvm_value, expr->subscript.index_const, "expr.subscript"); break;
                case IR_TYPE_KIND_ARRAY: llvm_value = LLVMBuildStructGEP2(context->llvm_builder, ir_type_to_llvm(context, expr->subscript.value->type), value.llvm_value, expr->subscript.index_const, "expr.subscript"); break;
                case IR_TYPE_KIND_POINTER:
                    llvm_value = LLVMBuildGEP2(
                        context->llvm_builder,
                        ir_type_to_llvm(context, expr->type),
                        resolve_ref(context, expr->subscript.value->type, value),
                        (LLVMValueRef[]) { LLVMConstInt(LLVMInt64TypeInContext(context->llvm_context), expr->subscript.index_const, false) },
                        1,
                        "expr.subscript"
                    );
                    break;
                default: diag_error(expr->source_location, LANG_E_INVALID_TYPE);
            }
            break;
        }
        case IR_SUBSCRIPT_KIND_MEMBER: {
            for(size_t i = 0; i < expr->subscript.value->type->structure.member_count; i++) {
                ir_type_structure_member_t *member = &expr->subscript.value->type->structure.members[i];
                if(strcmp(member->name, expr->subscript.member) != 0) continue;
                llvm_value = LLVMBuildStructGEP2(context->llvm_builder, ir_type_to_llvm(context, expr->subscript.value->type), value.llvm_value, i, "expr.subscript");
                goto brk;
            }
            diag_error(expr->source_location, LANG_E_UNKNOWN_MEMBER, expr->subscript.member);
        brk:
            break;
        }
    }

    return (value_t) { .is_ref = true, .llvm_value = llvm_value };
}

static value_t cg_expr_selector(CG_EXPR_PARAMS) {
    return cg_expr_ext(context, expr->selector.value);
}

static value_t cg_expr_sizeof(CG_EXPR_PARAMS) {
    return (value_t) { .llvm_value = LLVMSizeOf(ir_type_to_llvm(context, expr->_sizeof.type)) };
}

static value_t cg_expr_enumeration_value(CG_EXPR_PARAMS) {
    return (value_t) { .llvm_value = LLVMConstInt(ir_type_to_llvm(context, expr->enumeration_value.enumeration->type), expr->enumeration_value.value, false) };
}

// Common

static void cg_stmt(CG_STMT_PARAMS) {
    switch(stmt->kind) {
        case IR_STMT_KIND_BLOCK:       cg_stmt_block(context, stmt); break;
        case IR_STMT_KIND_DECLARATION: cg_stmt_declaration(context, stmt); break;
        case IR_STMT_KIND_EXPRESSION:  cg_expr(context, stmt->expression.expression); break;
        case IR_STMT_KIND_RETURN:      cg_stmt_return(context, stmt); break;
        case IR_STMT_KIND_IF:          cg_stmt_if(context, stmt); break;
        case IR_STMT_KIND_WHILE:       cg_stmt_while(context, stmt); break;
        case IR_STMT_KIND_CONTINUE:    cg_stmt_continue(context, stmt); break;
        case IR_STMT_KIND_BREAK:       cg_stmt_break(context, stmt); break;
        case IR_STMT_KIND_FOR:         cg_stmt_for(context, stmt); break;
        case IR_STMT_KIND_SWITCH:      cg_stmt_switch(context, stmt); break;
    }
}

static LLVMValueRef cg_expr(CG_EXPR_PARAMS) {
    return resolve_ref(context, expr->type, cg_expr_ext(context, expr));
}

static value_t cg_expr_ext(CG_EXPR_PARAMS) {
    value_t value;
    switch(expr->kind) {
        case IR_EXPR_KIND_LITERAL_NUMERIC:   value = cg_expr_literal_numeric(context, expr); break;
        case IR_EXPR_KIND_LITERAL_STRING:    value = cg_expr_literal_string(context, expr); break;
        case IR_EXPR_KIND_LITERAL_CHAR:      value = cg_expr_literal_char(context, expr); break;
        case IR_EXPR_KIND_LITERAL_BOOL:      value = cg_expr_literal_bool(context, expr); break;
        case IR_EXPR_KIND_LITERAL_STRUCT:    value = cg_expr_literal_struct(context, expr); break;
        case IR_EXPR_KIND_BINARY:            value = cg_expr_binary(context, expr); break;
        case IR_EXPR_KIND_UNARY:             value = cg_expr_unary(context, expr); break;
        case IR_EXPR_KIND_VARIABLE:          value = cg_expr_variable(context, expr); break;
        case IR_EXPR_KIND_CALL:              value = cg_expr_call(context, expr); break;
        case IR_EXPR_KIND_TUPLE:             value = cg_expr_tuple(context, expr); break;
        case IR_EXPR_KIND_CAST:              value = cg_expr_cast(context, expr); break;
        case IR_EXPR_KIND_SUBSCRIPT:         value = cg_expr_subscript(context, expr); break;
        case IR_EXPR_KIND_SELECTOR:          value = cg_expr_selector(context, expr); break;
        case IR_EXPR_KIND_SIZEOF:            value = cg_expr_sizeof(context, expr); break;
        case IR_EXPR_KIND_ENUMERATION_VALUE: value = cg_expr_enumeration_value(context, expr); break;
    }
    return value;
}

// //
//
//

static void cg_function(context_t *context, ir_function_t *function) {
    if(function->is_extern) return;

    ir_function_prototype_t prototype = function->prototype;

    context->return_state.has_returned = false;
    context->return_state.wont_return = false;
    context->return_state.type = prototype.return_type;
    context->loop_state.in_loop = false;

    LLVMBasicBlockRef bb_entry = LLVMAppendBasicBlockInContext(context->llvm_context, function->codegen_data, "tlc.function");
    LLVMPositionBuilderAtEnd(context->llvm_builder, bb_entry);

    for(size_t i = 0; i < prototype.argument_count; i++) {
        ir_variable_t *param = function->arguments[i];
        param->codegen_data = LLVMBuildAlloca(context->llvm_builder, ir_type_to_llvm(context, param->type), param->name);
        LLVMBuildStore(context->llvm_builder, LLVMGetParam(function->codegen_data, i), param->codegen_data);
    }

    if(function->statement != NULL) cg_stmt(context, function->statement);

    if(!context->return_state.has_returned && !context->return_state.wont_return) {
        if(context->return_state.type->kind != IR_TYPE_KIND_VOID) diag_error(function->source_location, LANG_E_MISSING_RETURN);
        LLVMBuildRetVoid(context->llvm_builder);
    }
}

static void cg_global_variable(context_t *context, ir_variable_t *variable) {
    LLVMValueRef llvm_value = NULL;
    if(variable->initial_value != NULL) {
        llvm_value = cg_expr(context, variable->initial_value);
    } else {
        llvm_value = LLVMConstNull(ir_type_to_llvm(context, variable->type));
    }
    LLVMSetInitializer(variable->codegen_data, llvm_value);
}

static void cg_namespace(context_t *context, ir_namespace_t *namespace) {
    for(size_t i = 0; i < namespace->symbol_count; i++) {
        ir_symbol_t *symbol = namespace->symbols[i];
        switch(symbol->kind) {
            case IR_SYMBOL_KIND_MODULE:      cg_namespace(context, &symbol->module->namespace); break;
            case IR_SYMBOL_KIND_FUNCTION:    cg_function(context, symbol->function); break;
            case IR_SYMBOL_KIND_VARIABLE:    cg_global_variable(context, symbol->variable); break;
            case IR_SYMBOL_KIND_ENUMERATION: break;
        }
    }
}

static void populate_namespace(context_t *context, ir_module_t *current_module, ir_namespace_t *namespace) {
    for(size_t i = 0; i < namespace->symbol_count; i++) {
        ir_symbol_t *symbol = namespace->symbols[i];
        switch(symbol->kind) {
            case IR_SYMBOL_KIND_MODULE: populate_namespace(context, symbol->module, &symbol->module->namespace); break;
            case IR_SYMBOL_KIND_FUNCTION:
                const char *link_name = symbol->function->link_name;
                if(link_name == NULL) link_name = mangle_name(symbol->function->name, current_module);
                symbol->function->codegen_data = LLVMAddFunction(context->llvm_module, link_name, ir_function_prototype_to_llvm(context, symbol->function->prototype));
                break;
            case IR_SYMBOL_KIND_VARIABLE:
                LLVMTypeRef type = ir_type_to_llvm(context, symbol->variable->type);
                symbol->variable->codegen_data = LLVMAddGlobal(context->llvm_module, type, mangle_name(symbol->variable->name, current_module));
                break;
            case IR_SYMBOL_KIND_ENUMERATION: break;
        }
    }
}

codegen_context_t *codegen(ir_unit_t *unit, ir_type_cache_t *type_cache, codegen_optimization_t optimization, codegen_code_model_t code_model, codegen_relocation_t relocation_mode, const char *features) {
    LLVMCodeModel llvm_code_model;
    switch(code_model) {
        case CODEGEN_CODE_MODEL_DEFAULT: llvm_code_model = LLVMCodeModelDefault; break;
        case CODEGEN_CODE_MODEL_TINY:    llvm_code_model = LLVMCodeModelTiny; break;
        case CODEGEN_CODE_MODEL_SMALL:   llvm_code_model = LLVMCodeModelSmall; break;
        case CODEGEN_CODE_MODEL_MEDIUM:  llvm_code_model = LLVMCodeModelMedium; break;
        case CODEGEN_CODE_MODEL_LARGE:   llvm_code_model = LLVMCodeModelLarge; break;
        case CODEGEN_CODE_MODEL_KERNEL:  llvm_code_model = LLVMCodeModelKernel; break;
    }

    LLVMRelocMode llvm_reloc_model;
    switch(relocation_mode) {
        case CODEGEN_RELOCATION_DEFAULT: llvm_reloc_model = LLVMRelocDefault; break;
        case CODEGEN_RELOCATION_STATIC:  llvm_reloc_model = LLVMRelocStatic; break;
        case CODEGEN_RELOCATION_PIC:     llvm_reloc_model = LLVMRelocPIC; break;
    }

    const char *passes = NULL;
    switch(optimization) {
        case CODEGEN_OPTIMIZATION_NONE: break;
        case CODEGEN_OPTIMIZATION_O1:   passes = "default<O1>"; break;
        case CODEGEN_OPTIMIZATION_O2:   passes = "default<O2>"; break;
        case CODEGEN_OPTIMIZATION_O3:   passes = "default<O3>"; break;
    }

    // OPTIMIZE: this is kind of lazy
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllTargets();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();

    char *error_message;

    const char *triple = LLVMGetDefaultTargetTriple();

    LLVMTargetRef target;
    if(LLVMGetTargetFromTriple(triple, &target, &error_message) != 0) log_fatal("failed to create target (%s)", error_message);

    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, triple, "generic", features, LLVMCodeGenLevelDefault, llvm_reloc_model, llvm_code_model);
    if(machine == NULL) log_fatal("failed to create target machine");

    context_t context;
    context.llvm_context = LLVMContextCreate();
    context.llvm_builder = LLVMCreateBuilderInContext(context.llvm_context);
    context.llvm_module = LLVMModuleCreateWithNameInContext("CharonModule", context.llvm_context);
    context.unit = unit;
    context.type_cache = type_cache;

    populate_namespace(&context, NULL, &unit->root_namespace);
    cg_namespace(&context, &unit->root_namespace);

    if(LLVMVerifyModule(context.llvm_module, LLVMReturnStatusAction, &error_message)) {
        if(LLVMPrintModuleToFile(context.llvm_module, "charonc.errordump.ll", &error_message)) log_fatal("emit failed (%s)\n", error_message);
        log_fatal("failed to verify module (%s)", error_message);
    }
    LLVMDisposeMessage(error_message);

    if(passes != NULL) {
        LLVMErrorRef error = LLVMRunPasses(context.llvm_module, passes, NULL, LLVMCreatePassBuilderOptions());
        if(error != NULL) log_warning("llvm ir optimization failed (%s)", LLVMGetErrorMessage(error));
    }

    LLVMSetTarget(context.llvm_module, triple);
    char *layout = LLVMCopyStringRepOfTargetData(LLVMCreateTargetDataLayout(machine));
    LLVMSetDataLayout(context.llvm_module, layout);
    LLVMDisposeMessage(layout);

    codegen_context_t *codegen_context = alloc(sizeof(codegen_context_t));
    codegen_context->llvm_context = context.llvm_context;
    codegen_context->llvm_module = context.llvm_module;
    codegen_context->llvm_builder = context.llvm_builder;
    codegen_context->llvm_machine = machine;
    return codegen_context;
}

void codegen_emit(codegen_context_t *context, const char *path, bool ir) {
    char *error_message;
    if(ir) {
        if(LLVMPrintModuleToFile(context->llvm_module, path, &error_message)) log_fatal("emit failed (%s)\n", error_message);
    } else {
        if(LLVMTargetMachineEmitToFile(context->llvm_machine, context->llvm_module, path, LLVMObjectFile, &error_message) != 0) log_fatal("emit failed (%s)", error_message);
    }
}

int codegen_run(codegen_context_t *context) {
    char *error_message;
    LLVMExecutionEngineRef engine;
    if(LLVMCreateInterpreterForModule(&engine, context->llvm_module, &error_message)) log_fatal("failed to create execution engine (%s)", error_message);

    LLVMValueRef fn;
    if(LLVMFindFunction(engine, "main", &fn)) log_fatal("missing main function");
    return LLVMRunFunctionAsMain(engine, fn, 1, (const char *[]) { "CharonModule" }, (const char *[]) { NULL });
}

void codegen_dispose_context(codegen_context_t *context) {
    LLVMDisposeTargetMachine(context->llvm_machine);
    LLVMDisposeBuilder(context->llvm_builder);
    LLVMDisposeModule(context->llvm_module);
    LLVMContextDispose(context->llvm_context);
    alloc_free(context);
}
