#include "visit.h"

#include "ir/ir.h"

#include <assert.h>
#include <stddef.h>

#define NULLABLE(FN, V, ...)                         \
    if((V) != NULL) FN(V __VA_OPT__(, ) __VA_ARGS__)
#define VISIT_EXPR_EXT(EXPR, MODULE, FN, SCOPE) visit_expr(unit, type_cache, (MODULE), (FN), (SCOPE), (EXPR), visitor);
#define VISIT_EXPR(EXPR) VISIT_EXPR_EXT(EXPR, module, fn, scope)
#define VISIT_STMT_EXT(STMT, MODULE, FN, SCOPE) visit_stmt(unit, type_cache, (MODULE), (FN), (SCOPE), (STMT), visitor);
#define VISIT_STMT(STMT) VISIT_STMT_EXT(STMT, module, fn, scope)

static void visit_expr(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_function_t *fn, ir_scope_t *scope, ir_expr_t *expr, visitor_t *visitor) {
    switch(expr->kind) {
        case IR_EXPR_KIND_LITERAL_NUMERIC: break;
        case IR_EXPR_KIND_LITERAL_STRING:  break;
        case IR_EXPR_KIND_LITERAL_CHAR:    break;
        case IR_EXPR_KIND_LITERAL_BOOL:    break;
        case IR_EXPR_KIND_LITERAL_STRUCT:
            assert(expr->literal.struct_value.type->kind == IR_TYPE_KIND_STRUCTURE);
            for(size_t i = 0; i < expr->literal.struct_value.type->structure.member_count; i++) {
                ir_literal_struct_member_t *member = expr->literal.struct_value.members[i];
                if(member == NULL) continue;
                VISIT_EXPR(member->value);
            }
            break;
        case IR_EXPR_KIND_BINARY:
            VISIT_EXPR(expr->binary.left);
            VISIT_EXPR(expr->binary.right);
            break;
        case IR_EXPR_KIND_UNARY:    VISIT_EXPR(expr->unary.operand); break;
        case IR_EXPR_KIND_VARIABLE: break;
        case IR_EXPR_KIND_CALL:
            VISIT_EXPR(expr->call.function_reference);
            VECTOR_FOREACH(&expr->call.arguments, i, elem) VISIT_EXPR(elem);
            break;
        case IR_EXPR_KIND_TUPLE: VECTOR_FOREACH(&expr->tuple.values, i, elem) VISIT_EXPR(elem); break;
        case IR_EXPR_KIND_CAST:  VISIT_EXPR(expr->cast.value); break;
        case IR_EXPR_KIND_SUBSCRIPT:
            VISIT_EXPR(expr->subscript.value);
            if(expr->subscript.kind == IR_SUBSCRIPT_KIND_INDEX) VISIT_EXPR(expr->subscript.index);
            break;
        case IR_EXPR_KIND_SELECTOR:          VISIT_EXPR_EXT(expr->selector.value, expr->selector.module, fn, scope); break;
        case IR_EXPR_KIND_SIZEOF:            break;
        case IR_EXPR_KIND_ENUMERATION_VALUE: break;
    }

    if(visitor->expr != NULL) visitor->expr(unit, type_cache, module, fn, scope, expr);
}

static void visit_stmt(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_function_t *fn, ir_scope_t *scope, ir_stmt_t *stmt, visitor_t *visitor) {
    switch(stmt->kind) {
        case IR_STMT_KIND_BLOCK:       VECTOR_FOREACH(&stmt->block.statements, i, elem) VISIT_STMT_EXT(elem, module, fn, stmt->block.scope); break;
        case IR_STMT_KIND_DECLARATION: NULLABLE(VISIT_EXPR, stmt->declaration.variable->initial_value); break;
        case IR_STMT_KIND_EXPRESSION:  VISIT_EXPR(stmt->expression.expression); break;
        case IR_STMT_KIND_RETURN:      NULLABLE(VISIT_EXPR, stmt->_return.value); break;
        case IR_STMT_KIND_IF:
            VISIT_EXPR(stmt->_if.condition);
            NULLABLE(VISIT_STMT, stmt->_if.body);
            NULLABLE(VISIT_STMT, stmt->_if.else_body);
            break;
        case IR_STMT_KIND_WHILE:
            NULLABLE(VISIT_EXPR, stmt->_while.condition);
            NULLABLE(VISIT_STMT, stmt->_while.body);
            break;
        case IR_STMT_KIND_CONTINUE: break;
        case IR_STMT_KIND_BREAK:    break;
        case IR_STMT_KIND_FOR:
            NULLABLE(VISIT_STMT_EXT, stmt->_for.declaration, module, fn, stmt->_for.scope);
            NULLABLE(VISIT_EXPR_EXT, stmt->_for.condition, module, fn, stmt->_for.scope);
            NULLABLE(VISIT_EXPR_EXT, stmt->_for.expr_after, module, fn, stmt->_for.scope);
            NULLABLE(VISIT_STMT_EXT, stmt->_for.body, module, fn, stmt->_for.scope);
            break;
        case IR_STMT_KIND_SWITCH:
            VISIT_EXPR(stmt->_switch.value);
            VISIT_STMT_EXT(stmt->_switch.default_body, module, fn, stmt->_switch.default_body_scope);
            for(size_t i = 0; i < stmt->_switch.case_count; i++) {
                VISIT_EXPR(stmt->_switch.cases[i].value);
                VISIT_STMT_EXT(stmt->_switch.cases[i].body, module, fn, stmt->_switch.cases[i].body_scope);
            }
            break;
    }

    if(visitor->stmt != NULL) visitor->stmt(unit, type_cache, module, fn, scope, stmt);
}

static void visit_namespace(ir_unit_t *unit, ir_type_cache_t *type_cache, ir_module_t *module, ir_namespace_t *namespace, visitor_t *visitor) {
    for(size_t i = 0; i < namespace->symbol_count; i++) {
        ir_symbol_t *symbol = namespace->symbols[i];
        switch(symbol->kind) {
            case IR_SYMBOL_KIND_MODULE:
                visit_namespace(unit, type_cache, symbol->module, &symbol->module->namespace, visitor);
                if(visitor->module != NULL) visitor->module(unit, type_cache, symbol->module);
                break;
            case IR_SYMBOL_KIND_VARIABLE:
                NULLABLE(VISIT_EXPR_EXT, symbol->variable->initial_value, module, NULL, NULL);
                if(visitor->global_variable != NULL) visitor->global_variable(unit, type_cache, module, symbol->variable);
                break;
            case IR_SYMBOL_KIND_FUNCTION:
                if(!symbol->function->is_extern) NULLABLE(VISIT_STMT_EXT, symbol->function->statement, module, symbol->function, symbol->function->scope);
                if(visitor->function != NULL) visitor->function(unit, type_cache, module, symbol->function);
                break;
            case IR_SYMBOL_KIND_ENUMERATION:
                if(visitor->enumeration != NULL) visitor->enumeration(unit, type_cache, module, symbol->enumeration);
                break;
        }
    }
}

void visit(ir_unit_t *unit, ir_type_cache_t *type_cache, visitor_t *visitor) {
    visit_namespace(unit, type_cache, NULL, &unit->root_namespace, visitor);
}
