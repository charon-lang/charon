#include "pass.h"

#include "constants.h"
#include "lib/alloc.h"
#include "lib/diag.h"
#include "ir/visit.h"

#include <assert.h>

#define TYPE_BOOL ir_type_cache_get_integer(type_cache, 1, false, false)
#define TYPE_CHAR ir_type_cache_get_integer(type_cache, CONSTANTS_CHAR_SIZE, false, false)

static ir_expr_t *make_cast(ir_expr_t *expr, ir_type_t *type) {
    ir_expr_t *cast_expr = alloc(sizeof(ir_expr_t));
    cast_expr->source_location = expr->source_location;
    cast_expr->kind = IR_EXPR_KIND_CAST;
    cast_expr->is_const = expr->is_const;
    cast_expr->type = type;
    cast_expr->cast.type = type;
    cast_expr->cast.value = expr;
    return cast_expr;
}

static bool try_coerce(ir_expr_t **expr, ir_type_t *type_to) {
    ir_type_t *type_from = (*expr)->type;
    if(ir_type_eq(type_from, type_to)) return true;

    if(type_to->kind == IR_TYPE_KIND_INTEGER) {
        if(type_from->kind != IR_TYPE_KIND_INTEGER) return false;
        if(type_from->integer.is_signed != type_to->integer.is_signed) return false;
        if(type_from->integer.bit_size > type_to->integer.bit_size) return false;
        goto ok;
    }

    if(type_to->kind == IR_TYPE_KIND_POINTER) {
        switch(type_from->kind) {
            case IR_TYPE_KIND_ARRAY:
                if(!ir_type_eq(type_to->pointer.pointee, type_from->array.type)) return false;
                break;
            case IR_TYPE_KIND_INTEGER:
                if(!type_from->integer.allow_coerce_pointer) return false;
                break;
            default: return false;
        }
        goto ok;
    }

    return false;
ok:
    *expr = make_cast(*expr, type_to);
    return true;
}

static void visitor_global_variable(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_variable_t *variable) {
    if(variable->initial_value != NULL && !try_coerce(&variable->initial_value, variable->type)) diag_error(variable->initial_value->source_location, LANG_E_DECL_TYPES_MISMATCH);
}

static void visitor_stmt(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_function_t *fn,  ir_scope_t *scope, ir_stmt_t *stmt) {
    switch(stmt->kind) {
        case IR_STMT_KIND_DECLARATION: {
            ir_type_t *type = stmt->declaration.variable->type;
            if(stmt->declaration.variable->type == NULL) {
                assert(stmt->declaration.variable->initial_value != NULL);
                stmt->declaration.variable->type = stmt->declaration.variable->initial_value->type;
            } else {
                if(stmt->declaration.variable->initial_value != NULL && !try_coerce(&stmt->declaration.variable->initial_value, type)) diag_error(stmt->source_location, LANG_E_DECL_TYPES_MISMATCH);
            }
            break;
        }
        case IR_STMT_KIND_RETURN: {
            assert(fn != NULL);
            if(stmt->_return.value != NULL && !try_coerce(&stmt->_return.value, fn->prototype.return_type)) diag_error(stmt->source_location, LANG_E_INVALID_TYPE);
            break;
        }
        case IR_STMT_KIND_IF: {
            if(!try_coerce(&stmt->_if.condition, TYPE_BOOL)) diag_error(stmt->_if.condition->source_location, LANG_E_NOT_BOOL);
            break;
        }
        case IR_STMT_KIND_WHILE: {
            if(stmt->_while.condition != NULL && !try_coerce(&stmt->_while.condition, TYPE_BOOL)) diag_error(stmt->_while.condition->source_location, LANG_E_NOT_BOOL);
            break;
        }
        case IR_STMT_KIND_FOR: {
            if(stmt->_for.condition != NULL && !try_coerce(&stmt->_for.condition, TYPE_BOOL)) diag_error(stmt->_for.condition->source_location, LANG_E_NOT_BOOL);
            break;
        }
        default: break;
    }
}

static void visitor_expr(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_function_t *fn, ir_scope_t *scope, ir_expr_t *expr) {
    switch(expr->kind) {
        case IR_EXPR_KIND_LITERAL_NUMERIC: expr->type = ir_type_cache_get_integer(type_cache, CONSTANTS_INT_SIZE, false, false); break;
        case IR_EXPR_KIND_LITERAL_STRING: expr->type = ir_type_cache_get_array(type_cache, TYPE_CHAR, strlen(expr->literal.string_value) + 1); break;
        case IR_EXPR_KIND_LITERAL_CHAR: expr->type = TYPE_CHAR; break;
        case IR_EXPR_KIND_LITERAL_BOOL: expr->type = TYPE_BOOL; break;
        case IR_EXPR_KIND_BINARY: {
            if(expr->binary.operation == IR_BINARY_OPERATION_LOGICAL_AND || expr->binary.operation == IR_BINARY_OPERATION_LOGICAL_OR) {
                if(!try_coerce(&expr->binary.left, TYPE_BOOL)) diag_error(expr->binary.left->source_location, LANG_E_NOT_BOOL);
                if(!try_coerce(&expr->binary.right, TYPE_BOOL)) diag_error(expr->binary.right->source_location, LANG_E_NOT_BOOL);
                expr->type = TYPE_BOOL;
                break;
            }

            if(expr->binary.operation == IR_BINARY_OPERATION_ASSIGN) {
                if(!try_coerce(&expr->binary.right, expr->binary.left->type)) diag_error(expr->source_location, LANG_E_CONFLICTING_TYPES);
                expr->type = expr->binary.right->type;
                break;
            }

            ir_type_t *type = expr->binary.left->type;
            if(!try_coerce(&expr->binary.right, type)) {
                type = expr->binary.right->type;
                if(!try_coerce(&expr->binary.left, type)) diag_error(expr->source_location, LANG_E_CONFLICTING_TYPES);
            }

            if(expr->binary.operation == IR_BINARY_OPERATION_EQUAL || expr->binary.operation == IR_BINARY_OPERATION_NOT_EQUAL) {
                expr->type = TYPE_BOOL;
                break;
            }

            if(type->kind != IR_TYPE_KIND_INTEGER) diag_error(expr->source_location, LANG_E_INVALID_TYPE);

            switch(expr->binary.operation) {
                case IR_BINARY_OPERATION_ADDITION:
                case IR_BINARY_OPERATION_SUBTRACTION:
                case IR_BINARY_OPERATION_MULTIPLICATION:
                case IR_BINARY_OPERATION_DIVISION:
                case IR_BINARY_OPERATION_MODULO:

                case IR_BINARY_OPERATION_SHIFT_LEFT:
                case IR_BINARY_OPERATION_SHIFT_RIGHT:
                case IR_BINARY_OPERATION_AND:
                case IR_BINARY_OPERATION_OR:
                case IR_BINARY_OPERATION_XOR:
                    expr->type = type;
                    break;

                case IR_BINARY_OPERATION_GREATER:
                case IR_BINARY_OPERATION_GREATER_EQUAL:
                case IR_BINARY_OPERATION_LESS:
                case IR_BINARY_OPERATION_LESS_EQUAL:
                    expr->type = TYPE_BOOL;
                    break;

                default: assert(false);
            }
            break;
        }
        case IR_EXPR_KIND_UNARY: {
            switch(expr->unary.operation) {
                case IR_UNARY_OPERATION_DEREF:
                    if(expr->unary.operand->type->kind != IR_TYPE_KIND_POINTER) diag_error(expr->source_location, LANG_E_NOT_POINTER);
                    expr->type = expr->unary.operand->type->pointer.pointee;
                    break;
                case IR_UNARY_OPERATION_NOT:
                    if(expr->unary.operand->type->kind != IR_TYPE_KIND_INTEGER) diag_error(expr->source_location, LANG_E_NOT_NUMERIC);
                    expr->type = TYPE_BOOL;
                    break;
                case IR_UNARY_OPERATION_NEGATIVE:
                    if(expr->unary.operand->type->kind != IR_TYPE_KIND_INTEGER) diag_error(expr->source_location, LANG_E_NOT_NUMERIC);
                    expr->type = expr->unary.operand->type;
                    break;
                case IR_UNARY_OPERATION_REF:
                    expr->type = ir_type_cache_get_pointer(type_cache, expr->unary.operand->type);
                    break;
            }
            break;
        }
        case IR_EXPR_KIND_VARIABLE: {
            if(expr->variable.is_function) {
                expr->type = ir_type_cache_get_function_reference(type_cache, expr->variable.function->prototype);
                break;
            }

            assert(expr->variable.variable->type != NULL);
            expr->type = expr->variable.variable->type;
            break;
        }
        case IR_EXPR_KIND_CALL: {
            ir_type_t *type = expr->call.function_reference->type;
            if(type->kind != IR_TYPE_KIND_FUNCTION_REFERENCE) diag_error(expr->source_location, LANG_E_NOT_FUNCTION);

            size_t argument_count = VECTOR_SIZE(&expr->call.arguments);
            ir_function_prototype_t prototype = type->function_reference.prototype;

            if(argument_count < prototype.argument_count) diag_error(expr->source_location, LANG_E_MISSING_ARGS);
            if(!prototype.varargs && argument_count > prototype.argument_count) diag_error(expr->source_location, LANG_E_INVALID_NUMBER_ARGS);

            VECTOR_FOREACH(&expr->call.arguments, i, elem) {
                if(prototype.argument_count > i && !try_coerce(VECTOR_FOREACH_ELEMENT_REF(&expr->call.arguments, i), prototype.arguments[i])) diag_error(elem->source_location, LANG_E_INVALID_TYPE);
            };
            expr->type = prototype.return_type;
            break;
        }
        case IR_EXPR_KIND_TUPLE: {
            ir_type_t **types = alloc(VECTOR_SIZE(&expr->tuple.values) * sizeof(ir_type_t *));
            VECTOR_FOREACH(&expr->tuple.values, i, elem) types[i] = elem->type;
            expr->type = ir_type_cache_get_tuple(type_cache, VECTOR_SIZE(&expr->tuple.values), types);
            break;
        }
        case IR_EXPR_KIND_CAST: expr->type = expr->cast.type; break;
        case IR_EXPR_KIND_SUBSCRIPT: {
            switch(expr->subscript.kind) {
                case IR_SUBSCRIPT_KIND_INDEX: {
                    if(expr->subscript.index->type->kind != IR_TYPE_KIND_INTEGER) diag_error(expr->source_location, LANG_E_INVALID_TYPE);

                    switch(expr->subscript.value->type->kind) {
                        case IR_TYPE_KIND_ARRAY: expr->type = expr->subscript.value->type->array.type; break;
                        case IR_TYPE_KIND_POINTER: expr->type = expr->subscript.value->type->pointer.pointee; break;
                        default: diag_error(expr->source_location, LANG_E_INVALID_TYPE);
                    }
                    break;
                }
                case IR_SUBSCRIPT_KIND_INDEX_CONST: {
                    switch(expr->subscript.value->type->kind) {
                        case IR_TYPE_KIND_TUPLE:
                            if(expr->subscript.index_const >= expr->subscript.value->type->tuple.type_count) diag_error(expr->source_location, LANG_E_INDEX_OUT_BOUNDS);
                            expr->type = expr->subscript.value->type->tuple.types[expr->subscript.index_const];
                            break;
                        case IR_TYPE_KIND_ARRAY:
                            if(expr->subscript.index_const >= expr->subscript.value->type->array.size) diag_error(expr->source_location, LANG_E_INDEX_OUT_BOUNDS);
                            expr->type = expr->subscript.value->type->array.type;
                            break;
                        case IR_TYPE_KIND_POINTER: expr->type = expr->subscript.value->type->pointer.pointee; break;
                        default: diag_error(expr->source_location, LANG_E_INVALID_TYPE);
                    }
                    break;
                }
                case IR_SUBSCRIPT_KIND_MEMBER: {
                    if(expr->subscript.value->type->kind != IR_TYPE_KIND_STRUCTURE) diag_error(expr->source_location, LANG_E_NOT_STRUCT);

                    for(size_t i = 0; i < expr->subscript.value->type->structure.member_count; i++) {
                        ir_type_structure_member_t *member = &expr->subscript.value->type->structure.members[i];
                        if(strcmp(member->name, expr->subscript.member) != 0) continue;
                        expr->type = member->type;
                        goto brk;
                    }
                    diag_error(expr->source_location, LANG_E_UNKNOWN_MEMBER, expr->subscript.member);
                    break;
                }
            }
            break;
        }
        case IR_EXPR_KIND_SELECTOR: expr->type = expr->selector.value->type; break;
        case IR_EXPR_KIND_SIZEOF: expr->type = ir_type_cache_get_integer(type_cache, 64, false, false); break;
        case IR_EXPR_KIND_ENUMERATION_VALUE: expr->type = expr->enumeration_value.enumeration->type; break;
        brk:
    }
    assert(expr->type != NULL);
}

void pass_eval_types(ir_unit_t *unit, ir_type_cache_t *type_cache) {
    visitor_t visitor = {
        .global_variable = visitor_global_variable,
        .stmt = visitor_stmt,
        .expr = visitor_expr
    };
    visit(unit, type_cache, &visitor);
}