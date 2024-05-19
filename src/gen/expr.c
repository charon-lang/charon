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
    gen_value_t right = gen_expr(ctx, node->expr_binary.right);

    if(node->expr_binary.operation == IR_BINARY_OPERATION_ASSIGN) {
        switch(node->expr_binary.left->type) {
            case IR_NODE_TYPE_EXPR_VAR:
                gen_variable_t *var = gen_scope_get_variable(ctx->scope, node->expr_binary.left->expr_var.name);
                LLVMBuildStore(ctx->builder, right.value, var->value);
                return right;
            case IR_NODE_TYPE_EXPR_UNARY:
                assert(node->expr_binary.left->expr_unary.operation == IR_UNARY_OPERATION_DEREF);
                gen_value_t value = gen_expr(ctx, node->expr_binary.left->expr_unary.operand);
                LLVMBuildStore(ctx->builder, right.value, value.value);
                return right;
            default: assert(false);
        }
    }

    gen_value_t left = gen_expr(ctx, node->expr_binary.left);
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

    gen_value_t operand = gen_expr(ctx, node->expr_unary.operand);
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
    LLVMValueRef func_ref = LLVMGetNamedFunction(ctx->module, node->expr_call.name);
    if(func_ref == NULL) assert(false);
    LLVMValueRef args[node->expr_call.argument_count];
    for(size_t i = 0; i < node->expr_call.argument_count; i++) args[i] = gen_expr(ctx, node->expr_call.arguments[i]).value;
    return (gen_value_t) {
        .type = ir_type_get_uint(), // TODO: need to store functions in context for this
        .value = LLVMBuildCall2(ctx->builder, LLVMGlobalGetValueType(func_ref), func_ref, args, node->expr_call.argument_count, "")
    };
}

static gen_value_t gen_expr_cast(gen_context_t *ctx, ir_node_t *node) {
    gen_value_t value = gen_expr(ctx, node->expr_cast.value);

    ir_type_t *to_type = node->expr_cast.type;
    ir_type_t *from_type = value.type;

    LLVMTypeRef llvm_to_type = gen_llvm_type(ctx, to_type);
    LLVMTypeRef llvm_from_type = gen_llvm_type(ctx, from_type);
    if(LLVMGetTypeKind(llvm_to_type) != LLVMGetTypeKind(llvm_from_type)) diag_error(node->diag_loc, "cast of incompatible types");

    LLVMTargetDataRef data_layout = LLVMGetModuleDataLayout(ctx->module);
    unsigned long long to_size = LLVMSizeOfTypeInBits(data_layout, llvm_to_type);
    unsigned long long size = LLVMSizeOfTypeInBits(data_layout, llvm_from_type);

    LLVMValueRef v = value.value;
    if(size > to_size) v = LLVMBuildTruncOrBitCast(ctx->builder, v, llvm_to_type, "");
    if(to_size > size) v = LLVMBuildZExtOrBitCast(ctx->builder, v, llvm_to_type, "");
    return (gen_value_t) {
        .type = value.type,
        .value = v
    };
}

gen_value_t gen_expr(gen_context_t *ctx, ir_node_t *node) {
    switch(node->type) {
        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: return gen_expr_literal_numeric(ctx, node);
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: return gen_expr_literal_string(ctx, node);
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: return gen_expr_literal_char(ctx, node);
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: return gen_expr_literal_bool(ctx, node);
        case IR_NODE_TYPE_EXPR_BINARY: return gen_expr_binary(ctx, node);
        case IR_NODE_TYPE_EXPR_UNARY: return gen_expr_unary(ctx, node);
        case IR_NODE_TYPE_EXPR_VAR: return gen_expr_var(ctx, node);
        case IR_NODE_TYPE_EXPR_CALL: return gen_expr_call(ctx, node);
        case IR_NODE_TYPE_EXPR_CAST: return gen_expr_cast(ctx, node);
        default: assert(false); // TODO: possibly separate expressions and statements
    }
}