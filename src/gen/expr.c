#include "gen.h"

static gen_value_t gen_expr_literal_numeric(gen_context_t *ctx, ir_node_t *node) {
    return (gen_value_t) {
        .type = ir_type_get_u64(),
        .value = LLVMConstInt(ctx->types.int64, node->expr_literal.numeric_value, false)
    };
}

static gen_value_t gen_expr_literal_string(gen_context_t *ctx, ir_node_t *node) {
    return (gen_value_t) {
        .type = ir_type_make_pointer(ir_type_get_char()),
        .value = LLVMBuildGlobalString(ctx->builder, node->expr_literal.string_value, "")
    };
}

static gen_value_t gen_expr_literal_char(gen_context_t *ctx, ir_node_t *node) {
    return (gen_value_t) {
        .type = ir_type_get_char(),
        .value = LLVMConstInt(ctx->types.int8, node->expr_literal.char_value, false)
    };
}

static gen_value_t gen_expr_literal_bool(gen_context_t *ctx, ir_node_t *node) {
    return (gen_value_t) {
        .type = ir_type_get_bool(),
        .value = LLVMConstInt(ctx->types.int1, node->expr_literal.bool_value ? 1 : 0, false)
    };
}

static gen_value_t gen_expr_binary(gen_context_t *ctx, ir_node_t *node) {
    gen_value_t right = gen_expr(ctx, node->expr_binary.right, NULL); // TODO: NULL?
    if(ir_type_is_void(right.type)) diag_error(node->diag_loc, "rhs of binary expression is void");

    if(node->expr_binary.operation == IR_BINARY_OPERATION_ASSIGN) {
        switch(node->expr_binary.left->type) {
            case IR_NODE_TYPE_EXPR_VAR:
                gen_variable_t *var = gen_scope_get_variable(ctx->scope, node->expr_binary.left->expr_var.name);
                if(!ir_type_is_eq(var->type, right.type)) diag_error(node->diag_loc, "conflicting types in assignment");
                LLVMBuildStore(ctx->builder, right.value, var->value);
                return right;
            case IR_NODE_TYPE_EXPR_UNARY:
                assert(node->expr_binary.left->expr_unary.operation == IR_UNARY_OPERATION_DEREF);
                gen_value_t value = gen_expr(ctx, node->expr_binary.left->expr_unary.operand, right.type);
                LLVMBuildStore(ctx->builder, right.value, value.value);
                return right;
            default: assert(false);
        }
    }

    gen_value_t left = gen_expr(ctx, node->expr_binary.left, NULL); // TODO: NULL?
    if(!ir_type_is_eq(right.type, left.type)) diag_error(node->diag_loc, "conflicting types in binary expression");
    switch(node->expr_binary.operation) {
        case IR_BINARY_OPERATION_ADDITION: return (gen_value_t) {
            .type = left.type, // TODO
            .value = LLVMBuildAdd(ctx->builder, left.value, right.value, "")
        };
        case IR_BINARY_OPERATION_SUBTRACTION: return (gen_value_t) {
            .type = left.type, // TODO
            .value = LLVMBuildSub(ctx->builder, left.value, right.value, "")
        };
        case IR_BINARY_OPERATION_MULTIPLICATION: return (gen_value_t) {
            .type = left.type, // TODO
            .value = LLVMBuildMul(ctx->builder, left.value, right.value, "")
        };
        case IR_BINARY_OPERATION_DIVISION: return (gen_value_t) {
            .type = left.type, // TODO
            .value = LLVMBuildUDiv(ctx->builder, left.value, right.value, "")
        };
        case IR_BINARY_OPERATION_MODULO: return (gen_value_t) {
            .type = left.type, // TODO
            .value = LLVMBuildURem(ctx->builder, left.value, right.value, "")
        };
        case IR_BINARY_OPERATION_EQUAL: return (gen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(ctx->builder, LLVMIntEQ, left.value, right.value, "expr.binary.eq")
        };
        case IR_BINARY_OPERATION_NOT_EQUAL: return (gen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(ctx->builder, LLVMIntNE, left.value, right.value, "expr.binary.ne")
        };
        case IR_BINARY_OPERATION_GREATER: return (gen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(ctx->builder, LLVMIntUGT, left.value, right.value, "expr.binary.ugt")
        };
        case IR_BINARY_OPERATION_GREATER_EQUAL: return (gen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(ctx->builder, LLVMIntUGE, left.value, right.value, "expr.binary.uge")
        };
        case IR_BINARY_OPERATION_LESS: return (gen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(ctx->builder, LLVMIntULT, left.value, right.value, "expr.binary.ult")
        };
        case IR_BINARY_OPERATION_LESS_EQUAL: return (gen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(ctx->builder, LLVMIntULE, left.value, right.value, "expr.binary.ule")
        };
        default: assert(false);
    }
}

static gen_value_t gen_expr_unary(gen_context_t *ctx, ir_node_t *node) {
    if(node->expr_unary.operation == IR_UNARY_OPERATION_REF) {
        assert(node->expr_unary.operand->type == IR_NODE_TYPE_EXPR_VAR);
        gen_variable_t *var = gen_scope_get_variable(ctx->scope, node->expr_unary.operand->expr_var.name);
        return (gen_value_t) {
            .type = ir_type_make_pointer(var->type),
            .value = var->value
        };
    }

    gen_value_t operand = gen_expr(ctx, node->expr_unary.operand, NULL); // TODO: NULL?
    switch(node->expr_unary.operation) {
        case IR_UNARY_OPERATION_DEREF:
            assert(ir_type_is_kind(operand.type, IR_TYPE_KIND_POINTER));
            ir_type_t *type = operand.type->pointer.base;
            return (gen_value_t) {
                .type = type,
                .value = LLVMBuildLoad2(ctx->builder, gen_llvm_type(ctx, type), operand.value, "")
            };
        case IR_UNARY_OPERATION_NOT: return (gen_value_t) {
            .type = ir_type_get_bool(),
            .value = LLVMBuildICmp(ctx->builder, LLVMIntEQ, operand.value, LLVMConstInt(gen_llvm_type(ctx, operand.type), 0, false), "")
        };
        case IR_UNARY_OPERATION_NEGATIVE: return (gen_value_t) {
            .type = operand.type,
            .value = LLVMBuildNeg(ctx->builder, operand.value, "")
        };
        default: assert(false);
    }
}

static gen_value_t gen_expr_var(gen_context_t *ctx, ir_node_t *node) {
    gen_variable_t *var = gen_scope_get_variable(ctx->scope, node->expr_var.name);
    return (gen_value_t) {
        .type = var->type,
        .value = LLVMBuildLoad2(ctx->builder, gen_llvm_type(ctx, var->type), var->value, "")
    };
}

static gen_value_t gen_expr_call(gen_context_t *ctx, ir_node_t *node) {
    gen_function_t *function = gen_get_function(ctx, node->expr_call.name);
    if(function == NULL) diag_error(node->diag_loc, "reference to an undefined function '%s'", node->expr_call.name);
    if(node->expr_call.argument_count < function->type.argument_count) diag_error(node->diag_loc, "missing arguments");
    if(!function->type.varargs && node->expr_call.argument_count > function->type.argument_count) diag_error(node->diag_loc, "invalid number of arguments");
    LLVMValueRef args[node->expr_call.argument_count];
    for(size_t i = 0; i < node->expr_call.argument_count; i++) args[i] = gen_expr(ctx, node->expr_call.arguments[i], function->type.arguments[i]).value;
    return (gen_value_t) {
        .type = function->type.return_type,
        .value = LLVMBuildCall2(ctx->builder, function->llvm_type, function->value, args, node->expr_call.argument_count, "")
    };
}

static gen_value_t gen_expr_cast(gen_context_t *ctx, ir_node_t *node) {
    gen_value_t v = gen_expr(ctx, node->expr_cast.value, NULL);

    LLVMValueRef value = v.value;
    ir_type_t *to_type = node->expr_cast.type;
    ir_type_t *from_type = v.type;
    if(to_type->kind != from_type->kind) diag_error(node->diag_loc, "cast of incompatible types");

    LLVMTypeRef llvm_to_type = gen_llvm_type(ctx, to_type);

    switch(to_type->kind) {
        case IR_TYPE_KIND_VOID: diag_error(node->diag_loc, "void cast");
        case IR_TYPE_KIND_INTEGER:
            if(from_type->integer.bit_size == to_type->integer.bit_size) break;
            if(from_type->integer.bit_size > to_type->integer.bit_size) {
                value = LLVMBuildTrunc(ctx->builder, value, llvm_to_type, "cast.trunc");
            } else {
                if(to_type->integer.is_signed) {
                    value = LLVMBuildSExt(ctx->builder, value, llvm_to_type, "cast.sext");
                } else {
                    value = LLVMBuildZExt(ctx->builder, value, llvm_to_type, "cast.zext");
                }
            }
            break;
        case IR_TYPE_KIND_POINTER: break;
    }
    return (gen_value_t) { .type = to_type, .value = value };
}

gen_value_t gen_expr(gen_context_t *ctx, ir_node_t *node, ir_type_t *type_expected) {
    gen_value_t value;
    switch(node->type) {
        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: value = gen_expr_literal_numeric(ctx, node); break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: value = gen_expr_literal_string(ctx, node); break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: value = gen_expr_literal_char(ctx, node); break;
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: value = gen_expr_literal_bool(ctx, node); break;
        case IR_NODE_TYPE_EXPR_BINARY: value = gen_expr_binary(ctx, node); break;
        case IR_NODE_TYPE_EXPR_UNARY: value = gen_expr_unary(ctx, node); break;
        case IR_NODE_TYPE_EXPR_VAR: value = gen_expr_var(ctx, node); break;
        case IR_NODE_TYPE_EXPR_CALL: value = gen_expr_call(ctx, node); break;
        case IR_NODE_TYPE_EXPR_CAST: value = gen_expr_cast(ctx, node); break;
        default: assert(false); // TODO: possibly separate expressions and statements
    }
    if(type_expected != NULL && !ir_type_is_eq(value.type, type_expected)) diag_error(node->diag_loc, "conflicting types");
    return value;
}