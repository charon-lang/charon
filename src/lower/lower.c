#include "lower.h"

#include "constants.h"
#include "lib/diag.h"
#include "lib/alloc.h"

#include <assert.h>
#include <string.h>

typedef struct {
    hlir_type_t *base;
    const char **parameters;
    size_t parameter_count;
} generic_t;

typedef struct namespace_generics namespace_generics_t;
struct namespace_generics {
    struct {
        const char *name;
        generic_t generic;
    } *generic_entries;
    size_t generic_entry_count;

    struct {
        const char *name;
        namespace_generics_t *child;
    } *children;
    size_t child_count;
};

typedef struct {
    const char *name;
    hlir_type_t *type;
} generic_mapping_t;

struct lower_context {
    memory_allocator_t *work_allocator;
    ir_unit_t *unit;
    ir_type_cache_t *type_cache;
    namespace_generics_t *namespace_generics;
};

static ir_type_t *lower_type(lower_context_t *context, ir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, hlir_type_t *type, source_location_t source_location);
static ir_expr_t *lower_expr(lower_context_t *context, ir_namespace_t *current_namespace, ir_scope_t *scope, namespace_generics_t *current_namespace_generics, hlir_node_t *node);
static ir_stmt_t *lower_stmt(lower_context_t *context, ir_namespace_t *current_namespace, ir_scope_t *scope, namespace_generics_t *current_namespace_generics, hlir_node_t *node);

static namespace_generics_t *namespace_generics_find_child(namespace_generics_t *namespace_generics, const char *name) {
    for(size_t i = 0; i < namespace_generics->child_count; i++) {
        if(strcmp(namespace_generics->children[i].name, name) != 0) continue;
        return namespace_generics->children[i].child;
    }
    return NULL;
}

static namespace_generics_t *namespace_generics_make(lower_context_t *context, const char *name, namespace_generics_t *parent) {
    namespace_generics_t *namespace_generics = memory_allocate(context->work_allocator, sizeof(namespace_generics_t));
    namespace_generics->child_count = 0;
    namespace_generics->children = NULL;
    namespace_generics->generic_entry_count = 0;
    namespace_generics->generic_entries = NULL;
    if(parent != NULL) {
        parent->children = memory_allocate_array(context->work_allocator, parent->children, ++parent->child_count, sizeof(typeof(parent->children[0])));
        parent->children[parent->child_count - 1].name = name;
        parent->children[parent->child_count - 1].child = namespace_generics;
    }
    return namespace_generics;
}

static void namespace_generics_add(lower_context_t *context, namespace_generics_t *namespace_generics, const char *name, generic_t generic) {
    namespace_generics->generic_entries = memory_allocate_array(context->work_allocator, namespace_generics->generic_entries, ++namespace_generics->generic_entry_count, sizeof(typeof(namespace_generics->generic_entries[0])));
    namespace_generics->generic_entries[namespace_generics->generic_entry_count - 1].name = name;
    namespace_generics->generic_entries[namespace_generics->generic_entry_count - 1].generic = generic;
}

static generic_t *namespace_generics_find_generic(namespace_generics_t *namespace_generics, const char *name) {
    for(size_t i = 0; i < namespace_generics->generic_entry_count; i++) {
        if(strcmp(namespace_generics->generic_entries[i].name, name) != 0) continue;
        return &namespace_generics->generic_entries[i].generic;
    }
    return NULL;
}

static ir_function_prototype_t lower_function_type(lower_context_t *context, ir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, hlir_type_function_t *function_type, source_location_t source_location) {
    ir_type_t **arguments = alloc_array(NULL, function_type->argument_count, sizeof(ir_type_t *));
    for(size_t i = 0; i < function_type->argument_count; i++) arguments[i] = lower_type(context, current_namespace, current_namespace_generics, function_type->arguments[i], source_location);
    return (ir_function_prototype_t) {
        .argument_count = function_type->argument_count,
        .arguments = arguments,
        .return_type = lower_type(context, current_namespace, current_namespace_generics, function_type->return_type, source_location),
        .varargs = function_type->varargs
    };
}

static ir_type_t *lower_type_ext(lower_context_t *context, ir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, generic_mapping_t *generic_mappings, size_t generic_mapping_count, hlir_type_t *type, source_location_t source_location) {
    if(type->is_reference) {
        if(type->reference.module_count == 0) {
            for(size_t i = 0; i < generic_mapping_count; i++) {
                if(strcmp(generic_mappings[i].name, type->reference.type_name) != 0) continue;
                return lower_type(context, current_namespace, current_namespace_generics, generic_mappings[i].type, source_location);
            }
        }

        for(size_t i = 0; i < type->reference.module_count; i++) {
            ir_symbol_t *symbol = ir_namespace_find_symbol_of_kind(current_namespace, type->reference.modules[i], IR_SYMBOL_KIND_MODULE);
            if(symbol == NULL) {
                symbol = ir_namespace_find_symbol_of_kind(&context->unit->root_namespace, type->reference.modules[i], IR_SYMBOL_KIND_MODULE);
                if(symbol == NULL) diag_error(source_location, "unknown module `%s`", type->reference.modules[i]);
            }
            current_namespace = &symbol->module->namespace;
            current_namespace_generics = namespace_generics_find_child(current_namespace_generics, type->reference.modules[i]);
            assert(current_namespace_generics != NULL);
        }

        generic_t *generic = namespace_generics_find_generic(current_namespace_generics, type->reference.type_name);
        ir_type_declaration_t *referred_type_decl = ir_namespace_find_type(current_namespace, type->reference.type_name);

        if(type->reference.generic_parameter_count > 0) {
            if(generic == NULL) diag_error(source_location, "unknown generic");
            if(generic->parameter_count != type->reference.generic_parameter_count) diag_error(source_location, "invalid number of parameters to generic");

            generic_mapping_t mappings[generic->parameter_count];
            for(size_t i = 0; i < generic->parameter_count; i++) mappings[i] = (generic_mapping_t) { .name = generic->parameters[i], .type = type->reference.generic_parameters[i] };
            return lower_type_ext(context, current_namespace, current_namespace_generics, mappings, generic->parameter_count, generic->base, source_location);
        } else {
            if(generic != NULL) diag_error(source_location, "type is a generic");
            if(referred_type_decl == NULL) diag_error(source_location, "unknown type `%s`", type->reference.type_name);
            return referred_type_decl->type;
        }
    }

    ir_type_t *new_type = NULL;
    switch(type->kind) {
        case HLIR_TYPE_KIND_VOID:
            new_type = ir_type_cache_get_void(context->type_cache);
            break;
        case HLIR_TYPE_KIND_INTEGER:
            hlir_attribute_t *attr_allow_coerce_pointer = hlir_attribute_find(&type->attributes, "allow_coerce_pointer", NULL, 0);
            new_type = ir_type_cache_get_integer(context->type_cache, type->integer.bit_size, type->integer.is_signed, attr_allow_coerce_pointer != NULL);
            break;
        case HLIR_TYPE_KIND_POINTER:
            new_type = ir_type_cache_get_pointer(context->type_cache, lower_type_ext(context, current_namespace, current_namespace_generics, generic_mappings, generic_mapping_count, type->pointer.pointee, source_location));
            break;
        case HLIR_TYPE_KIND_TUPLE: {
            ir_type_t **types = alloc_array(NULL, type->tuple.type_count, sizeof(ir_type_t *));
            for(size_t i = 0; i < type->tuple.type_count; i++) types[i] = lower_type_ext(context, current_namespace, current_namespace_generics, generic_mappings, generic_mapping_count, type->tuple.types[i], source_location);
            new_type = ir_type_cache_get_tuple(context->type_cache, type->tuple.type_count, types);
            break;
        }
        case HLIR_TYPE_KIND_ARRAY:
            new_type = ir_type_cache_get_array(context->type_cache, lower_type_ext(context, current_namespace, current_namespace_generics, generic_mappings, generic_mapping_count, type->array.type, source_location), type->array.size);
            break;
        case HLIR_TYPE_KIND_STRUCTURE: {
            hlir_attribute_t *attr_packed = hlir_attribute_find(&type->attributes, "packed", NULL, 0);
            ir_type_structure_member_t *members = alloc_array(NULL, type->structure.member_count, sizeof(ir_type_structure_member_t));
            for(size_t i = 0; i < type->structure.member_count; i++) {
                for(size_t j = i + 1; j < type->structure.member_count; j++) if(strcmp(type->structure.members[i].name, type->structure.members[j].name) == 0) diag_error(source_location, "duplicate member `%s`", type->structure.members[i].name);
                members[i] = (ir_type_structure_member_t) {
                    .name = alloc_strdup(type->structure.members[i].name),
                    .type = lower_type_ext(context, current_namespace, current_namespace_generics, generic_mappings, generic_mapping_count, type->structure.members[i].type, source_location)
                };
                // TODO we need to confirm we arent doing recursive embedding somehow
            }
            new_type = ir_type_cache_get_structure(context->type_cache, type->structure.member_count, members, attr_packed != NULL);
            break;
        }
        case HLIR_TYPE_KIND_FUNCTION_REFERENCE: {
            new_type = ir_type_cache_get_function_reference(context->type_cache, lower_function_type(context, current_namespace, current_namespace_generics, type->function_reference.function_type, source_location));
            break;
        }
    }
    assert(new_type != NULL);
    for(size_t i = 0; i < type->attributes.attribute_count; i++) {
        if(type->attributes.attributes[i].consumed) continue;
        diag_warn(type->attributes.attributes[i].source_location, "unhandled attribute");
    }
    return new_type;
}

static ir_type_t *lower_type(lower_context_t *context, ir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, hlir_type_t *type, source_location_t source_location) {
    return lower_type_ext(context, current_namespace, current_namespace_generics, NULL, 0, type, source_location);
}

static void lower_tlc(lower_context_t *context, ir_namespace_t *current_namespace, ir_scope_t *scope, namespace_generics_t *current_namespace_generics, hlir_node_t *node) {
    assert(node != NULL);

    switch(node->type) {
        case HLIR_NODE_TYPE_ROOT: HLIR_NODE_LIST_FOREACH(&node->root.tlcs, lower_tlc(context, current_namespace, scope, current_namespace_generics, node)); break;

        case HLIR_NODE_TYPE_TLC_MODULE: {
            ir_symbol_t *symbol = ir_namespace_find_symbol_of_kind(current_namespace, node->tlc_module.name, IR_SYMBOL_KIND_MODULE);
            assert(symbol != NULL);

            namespace_generics_t *child_generics = namespace_generics_find_child(current_namespace_generics, node->tlc_module.name);
            assert(child_generics != NULL);

            HLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, lower_tlc(context, &symbol->module->namespace, scope, child_generics, node));
            break;
        }
        case HLIR_NODE_TYPE_TLC_FUNCTION: {
            ir_symbol_t *symbol = ir_namespace_find_symbol_of_kind(current_namespace, node->tlc_function.name, IR_SYMBOL_KIND_FUNCTION);
            assert(symbol != NULL);

            ir_scope_t *new_scope = alloc(sizeof(ir_scope_t));
            new_scope->parent = scope;
            new_scope->variable_count = 0;
            new_scope->variables = NULL;

            ir_variable_t **arguments = alloc_array(NULL, node->tlc_function.function_type->argument_count, sizeof(ir_variable_t *));
            for(size_t i = 0; i < node->tlc_function.function_type->argument_count; i++) {
                arguments[i] = ir_scope_add(new_scope, alloc_strdup(node->tlc_function.argument_names[i]), symbol->function->prototype.arguments[i], NULL);
            }

            symbol->function->scope = new_scope;
            symbol->function->arguments = arguments;
            symbol->function->statement = node->tlc_function.statement == NULL ? NULL : lower_stmt(context, current_namespace, new_scope, current_namespace_generics, node->tlc_function.statement);
            break;
        }
        case HLIR_NODE_TYPE_TLC_DECLARATION: {
            ir_symbol_t *symbol = ir_namespace_find_symbol_of_kind(current_namespace, node->tlc_declaration.name, IR_SYMBOL_KIND_VARIABLE);
            assert(symbol != NULL);

            ir_expr_t *initial = NULL;
            if(node->tlc_declaration.initial != NULL) {
                initial = lower_expr(context, current_namespace, scope, current_namespace_generics, node->tlc_declaration.initial);
                if(!initial->is_const) diag_error(node->source_location, "cannot initialize global variable to a non-constant value");
            }
            symbol->variable->initial_value = initial;
            break;
        }
        case HLIR_NODE_TYPE_TLC_EXTERN: break;
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION: break;
        case HLIR_NODE_TYPE_TLC_ENUMERATION: break;
        default: assert(false);
    }
    for(size_t i = 0; i < node->attributes.attribute_count; i++) {
        if(node->attributes.attributes[i].consumed) continue;
        diag_warn(node->attributes.attributes[i].source_location, "unhandled attribute");
    }
}

static ir_stmt_t *lower_stmt(lower_context_t *context, ir_namespace_t *current_namespace, ir_scope_t *scope, namespace_generics_t *current_namespace_generics, hlir_node_t *node) {
    assert(node != NULL);

    ir_stmt_t *stmt = alloc(sizeof(ir_stmt_t));
    stmt->source_location = node->source_location;

    switch(node->type) {
        case HLIR_NODE_TYPE_STMT_BLOCK: {
            ir_scope_t *new_scope = alloc(sizeof(ir_scope_t));
            new_scope->parent = scope;
            new_scope->variable_count = 0;
            new_scope->variables = NULL;

            vector_stmt_t statements = vector_stmt_init();
            HLIR_NODE_LIST_FOREACH(&node->stmt_block.statements, vector_stmt_append(&statements, lower_stmt(context, current_namespace, new_scope, current_namespace_generics, node)));

            stmt->kind = IR_STMT_KIND_BLOCK;
            stmt->block.scope = new_scope;
            stmt->block.statements = statements;
            break;
        }
        case HLIR_NODE_TYPE_STMT_DECLARATION: {
            ir_variable_t *variable = ir_scope_find_variable(scope, node->stmt_declaration.name);
            if(variable != NULL) {
                if(variable->is_local && variable->scope == scope) diag_error(node->source_location, "redeclaration of `%s`", node->stmt_declaration.name);
                diag_warn(node->source_location, "declaration shadows `%s`", node->stmt_declaration.name);
            }

            ir_type_t *type = node->stmt_declaration.type == NULL ? NULL : lower_type(context, current_namespace, current_namespace_generics, node->stmt_declaration.type, node->source_location);
            ir_expr_t *initial = node->stmt_declaration.initial == NULL ? NULL : lower_expr(context, current_namespace, scope, current_namespace_generics, node->stmt_declaration.initial);
            if(type == NULL && initial == NULL) diag_error(node->source_location, "declaration is missing both an explicit and inferred type");

            stmt->kind = IR_STMT_KIND_DECLARATION;
            stmt->declaration.variable = ir_scope_add(scope, alloc_strdup(node->stmt_declaration.name), type, initial);
            break;
        }
        case HLIR_NODE_TYPE_STMT_EXPRESSION: {
            stmt->kind = IR_STMT_KIND_EXPRESSION;
            stmt->expression.expression = lower_expr(context, current_namespace, scope, current_namespace_generics, node->stmt_expression.expression);
            break;
        }
        case HLIR_NODE_TYPE_STMT_RETURN: {
            stmt->kind = IR_STMT_KIND_RETURN;
            stmt->_return.value = node->stmt_return.value == NULL ? NULL : lower_expr(context, current_namespace, scope, current_namespace_generics, node->stmt_return.value);
            break;
        }
        case HLIR_NODE_TYPE_STMT_IF: {
            stmt->kind = IR_STMT_KIND_IF;
            stmt->_if.condition = lower_expr(context, current_namespace, scope, current_namespace_generics, node->stmt_if.condition);
            stmt->_if.body = node->stmt_if.body == NULL ? NULL : lower_stmt(context, current_namespace, scope, current_namespace_generics, node->stmt_if.body);
            stmt->_if.else_body = node->stmt_if.else_body == NULL ? NULL : lower_stmt(context, current_namespace, scope, current_namespace_generics, node->stmt_if.else_body);
            break;
        }
        case HLIR_NODE_TYPE_STMT_WHILE: {
            stmt->kind = IR_STMT_KIND_WHILE;
            stmt->_while.condition = node->stmt_while.condition == NULL ? NULL : lower_expr(context, current_namespace, scope, current_namespace_generics, node->stmt_while.condition);
            stmt->_while.body = node->stmt_while.body == NULL ? NULL : lower_stmt(context, current_namespace, scope, current_namespace_generics, node->stmt_while.body);
            break;
        }
        case HLIR_NODE_TYPE_STMT_CONTINUE: stmt->kind = IR_STMT_KIND_CONTINUE; break;
        case HLIR_NODE_TYPE_STMT_BREAK: stmt->kind = IR_STMT_KIND_BREAK; break;
        case HLIR_NODE_TYPE_STMT_FOR: {
            ir_scope_t *new_scope = alloc(sizeof(ir_scope_t));
            new_scope->parent = scope;
            new_scope->variable_count = 0;
            new_scope->variables = NULL;

            stmt->kind = IR_STMT_KIND_FOR;
            stmt->_for.scope = new_scope;
            stmt->_for.declaration = node->stmt_for.declaration == NULL ? NULL : lower_stmt(context, current_namespace, new_scope, current_namespace_generics, node->stmt_for.declaration);
            stmt->_for.condition = node->stmt_for.condition == NULL ? NULL : lower_expr(context, current_namespace, new_scope, current_namespace_generics, node->stmt_for.condition);
            stmt->_for.expr_after = node->stmt_for.expr_after == NULL ? NULL : lower_expr(context, current_namespace, new_scope, current_namespace_generics, node->stmt_for.expr_after);
            stmt->_for.body = node->stmt_for.body == NULL ? NULL : lower_stmt(context, current_namespace, new_scope, current_namespace_generics, node->stmt_for.body);
            break;
        }
        default: assert(false);
    }
    for(size_t i = 0; i < node->attributes.attribute_count; i++) {
        if(node->attributes.attributes[i].consumed) continue;
        diag_warn(node->attributes.attributes[i].source_location, "unhandled attribute");
    }
    return stmt;
}

static ir_expr_t *lower_expr(lower_context_t *context, ir_namespace_t *current_namespace, ir_scope_t *scope, namespace_generics_t *current_namespace_generics, hlir_node_t *node) {
    assert(node != NULL);

    ir_expr_t *expr = alloc(sizeof(ir_expr_t));
    expr->source_location = node->source_location;
    expr->type = NULL;
    expr->is_const = false;

    switch(node->type) {
        case HLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC: {
            expr->kind = IR_EXPR_KIND_LITERAL_NUMERIC;
            expr->is_const = true;
            expr->literal.numeric_value = node->expr_literal.numeric_value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_LITERAL_STRING: {
            expr->kind = IR_EXPR_KIND_LITERAL_STRING;
            expr->is_const = true;
            expr->literal.string_value = alloc_strdup(node->expr_literal.string_value);
            break;
        }
        case HLIR_NODE_TYPE_EXPR_LITERAL_CHAR: {
            expr->kind = IR_EXPR_KIND_LITERAL_CHAR;
            expr->is_const = true;
            expr->literal.char_value = node->expr_literal.char_value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_LITERAL_BOOL: {
            expr->kind = IR_EXPR_KIND_LITERAL_BOOL;
            expr->type = NULL;
            expr->is_const = true;
            expr->literal.bool_value = node->expr_literal.bool_value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_BINARY: {
            ir_binary_operation_t operation;
            switch(node->expr_binary.operation) {
                case HLIR_NODE_BINARY_OPERATION_ASSIGN: operation = IR_BINARY_OPERATION_ASSIGN; break;
                case HLIR_NODE_BINARY_OPERATION_ADDITION: operation = IR_BINARY_OPERATION_ADDITION; break;
                case HLIR_NODE_BINARY_OPERATION_SUBTRACTION: operation = IR_BINARY_OPERATION_SUBTRACTION; break;
                case HLIR_NODE_BINARY_OPERATION_MULTIPLICATION: operation = IR_BINARY_OPERATION_MULTIPLICATION; break;
                case HLIR_NODE_BINARY_OPERATION_DIVISION: operation = IR_BINARY_OPERATION_DIVISION; break;
                case HLIR_NODE_BINARY_OPERATION_MODULO: operation = IR_BINARY_OPERATION_MODULO; break;
                case HLIR_NODE_BINARY_OPERATION_GREATER: operation = IR_BINARY_OPERATION_GREATER; break;
                case HLIR_NODE_BINARY_OPERATION_GREATER_EQUAL: operation = IR_BINARY_OPERATION_GREATER_EQUAL; break;
                case HLIR_NODE_BINARY_OPERATION_LESS: operation = IR_BINARY_OPERATION_LESS; break;
                case HLIR_NODE_BINARY_OPERATION_LESS_EQUAL: operation = IR_BINARY_OPERATION_LESS_EQUAL; break;
                case HLIR_NODE_BINARY_OPERATION_EQUAL: operation = IR_BINARY_OPERATION_EQUAL; break;
                case HLIR_NODE_BINARY_OPERATION_NOT_EQUAL: operation = IR_BINARY_OPERATION_NOT_EQUAL; break;
                case HLIR_NODE_BINARY_OPERATION_LOGICAL_AND: operation = IR_BINARY_OPERATION_LOGICAL_AND; break;
                case HLIR_NODE_BINARY_OPERATION_LOGICAL_OR: operation = IR_BINARY_OPERATION_LOGICAL_OR; break;
                case HLIR_NODE_BINARY_OPERATION_SHIFT_LEFT: operation = IR_BINARY_OPERATION_SHIFT_LEFT; break;
                case HLIR_NODE_BINARY_OPERATION_SHIFT_RIGHT: operation = IR_BINARY_OPERATION_SHIFT_RIGHT; break;
                case HLIR_NODE_BINARY_OPERATION_AND: operation = IR_BINARY_OPERATION_AND; break;
                case HLIR_NODE_BINARY_OPERATION_OR: operation = IR_BINARY_OPERATION_OR; break;
                case HLIR_NODE_BINARY_OPERATION_XOR: operation = IR_BINARY_OPERATION_XOR; break;
            }

            ir_expr_t *left = lower_expr(context, current_namespace, scope, current_namespace_generics, node->expr_binary.left);
            ir_expr_t *right = lower_expr(context, current_namespace, scope, current_namespace_generics, node->expr_binary.right);

            expr->kind = IR_EXPR_KIND_BINARY;
            expr->is_const = left->is_const && right->is_const;
            expr->binary.operation = operation;
            expr->binary.left = left;
            expr->binary.right = right;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_UNARY: {
            ir_unary_operation_t operation;
            switch(node->expr_unary.operation) {
                case HLIR_NODE_UNARY_OPERATION_NOT: operation = IR_UNARY_OPERATION_NOT; break;
                case HLIR_NODE_UNARY_OPERATION_NEGATIVE: operation = IR_UNARY_OPERATION_NEGATIVE; break;
                case HLIR_NODE_UNARY_OPERATION_DEREF: operation = IR_UNARY_OPERATION_DEREF; break;
                case HLIR_NODE_UNARY_OPERATION_REF: operation = IR_UNARY_OPERATION_REF; break;
            }

            ir_expr_t *operand = lower_expr(context, current_namespace, scope, current_namespace_generics, node->expr_unary.operand);

            expr->kind = IR_EXPR_KIND_UNARY;
            expr->is_const = operand->is_const;
            expr->unary.operation = operation;
            expr->unary.operand = operand;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_VARIABLE: {
            expr->kind = IR_EXPR_KIND_VARIABLE;
            expr->is_const = false; // TODO: if we can resolve const variables here... could be

            ir_variable_t *variable = ir_scope_find_variable(scope, node->expr_variable.name);
            if(variable != NULL) {
                expr->variable.is_function = false;
                expr->variable.variable = variable;
                break;
            }

            ir_symbol_t *symbol = ir_namespace_find_symbol(current_namespace, node->expr_variable.name);
            if(symbol == NULL) symbol = ir_namespace_find_symbol(&context->unit->root_namespace, node->expr_variable.name);
            if(symbol == NULL) diag_error(node->source_location, "referenced an undefined variable `%s`", node->expr_variable.name);
            switch(symbol->kind) {
                case IR_SYMBOL_KIND_FUNCTION:
                    expr->variable.is_function = true;
                    expr->variable.function = symbol->function;
                    break;
                case IR_SYMBOL_KIND_VARIABLE:
                    expr->variable.is_function = false;
                    expr->variable.variable = symbol->variable;
                    break;
                default: diag_error(node->source_location, "not a variable");
            }
            break;
        }
        case HLIR_NODE_TYPE_EXPR_TUPLE: {
            vector_expr_t values = vector_expr_init();
            HLIR_NODE_LIST_FOREACH(&node->expr_tuple.values, vector_expr_append(&values, lower_expr(context, current_namespace, scope, current_namespace_generics, node)));

            expr->kind = IR_EXPR_KIND_TUPLE;
            expr->is_const = true;
            VECTOR_FOREACH(&values, i, elem) expr->is_const = expr->is_const && elem->is_const;
            expr->tuple.values = values;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_CALL: {
            vector_expr_t arguments = vector_expr_init();
            HLIR_NODE_LIST_FOREACH(&node->expr_call.arguments, vector_expr_append(&arguments, lower_expr(context, current_namespace, scope, current_namespace_generics, node)));

            expr->kind = IR_EXPR_KIND_CALL;
            expr->call.function_reference = lower_expr(context, current_namespace, scope, current_namespace_generics, node->expr_call.function_reference);
            expr->call.arguments = arguments;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_CAST: {
            ir_expr_t *value = lower_expr(context, current_namespace, scope, current_namespace_generics, node->expr_cast.value);

            expr->kind = IR_EXPR_KIND_CAST;
            expr->is_const = value->is_const;
            expr->cast.type = lower_type(context, current_namespace, current_namespace_generics, node->expr_cast.type, node->source_location);
            expr->cast.value = value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_SUBSCRIPT: {
            ir_expr_t *value = lower_expr(context, current_namespace, scope, current_namespace_generics, node->expr_subscript.value);

            expr->kind = IR_EXPR_KIND_SUBSCRIPT;
            expr->subscript.value = value;
            switch(node->expr_subscript.type) {
                case HLIR_NODE_SUBSCRIPT_TYPE_INDEX:
                    expr->subscript.kind = IR_SUBSCRIPT_KIND_INDEX;
                    expr->subscript.index = lower_expr(context, current_namespace, scope, current_namespace_generics, node->expr_subscript.index);
                    break;
                case HLIR_NODE_SUBSCRIPT_TYPE_INDEX_CONST:
                    expr->is_const = value->is_const;
                    expr->subscript.kind = IR_SUBSCRIPT_KIND_INDEX_CONST;
                    expr->subscript.index_const = node->expr_subscript.index_const;
                    break;
                case HLIR_NODE_SUBSCRIPT_TYPE_MEMBER:
                    expr->is_const = value->is_const;
                    expr->subscript.kind = IR_SUBSCRIPT_KIND_MEMBER;
                    expr->subscript.member = alloc_strdup(node->expr_subscript.member);
                    break;
            }
            break;
        }
        case HLIR_NODE_TYPE_EXPR_SELECTOR: {
            ir_symbol_t *symbol = ir_namespace_find_symbol_of_kind(current_namespace, node->expr_selector.name, IR_SYMBOL_KIND_MODULE);
            if(symbol == NULL) {
                symbol = ir_namespace_find_symbol_of_kind(&context->unit->root_namespace, node->expr_selector.name, IR_SYMBOL_KIND_MODULE);
                if(symbol == NULL) goto enumeration;
            }

            namespace_generics_t *child_generics = namespace_generics_find_child(current_namespace_generics, node->expr_selector.name);
            assert(child_generics != NULL);

            ir_expr_t *value = lower_expr(context, &symbol->module->namespace, scope, child_generics, node->expr_selector.value);

            expr->kind = IR_EXPR_KIND_SELECTOR;
            expr->is_const = value->is_const;
            expr->selector.module = symbol->module;
            expr->selector.value = value;
            break;
        }
        enumeration: {
            ir_symbol_t *symbol = ir_namespace_find_symbol_of_kind(current_namespace, node->expr_selector.name, IR_SYMBOL_KIND_ENUMERATION);
            if(symbol == NULL) {
                symbol = ir_namespace_find_symbol_of_kind(&context->unit->root_namespace, node->expr_selector.name, IR_SYMBOL_KIND_ENUMERATION);
                if(symbol == NULL) diag_error(node->source_location, "unknown selector `%s`", node->expr_selector.name);
            }

            if(node->expr_selector.value->type != HLIR_NODE_TYPE_EXPR_VARIABLE) diag_error(node->source_location, "invalid operation on enum");

            for(size_t i = 0; i < symbol->enumeration->type->enumeration.member_count; i++) {
                if(strcmp(node->expr_selector.value->expr_variable.name, symbol->enumeration->members[i]) != 0) continue;
                expr->kind = IR_EXPR_KIND_ENUMERATION_VALUE;
                expr->is_const = true;
                expr->enumeration_value.value = i;
                expr->enumeration_value.enumeration = symbol->enumeration;
                goto enumeration_success;
            }
            diag_error(node->source_location, "unknown enum member");
            enumeration_success:
            break;
        }
        case HLIR_NODE_TYPE_EXPR_SIZEOF: {
            expr->kind = IR_EXPR_KIND_SIZEOF;
            expr->is_const = true;
            expr->_sizeof.type = lower_type(context, current_namespace, current_namespace_generics, node->expr_sizeof.type, node->source_location);
            break;
        }
        default: assert(false);
    }
    return expr;
}

static void populate_namespace_pass_two(lower_context_t *context, ir_module_t *current_module, namespace_generics_t *current_namespace_generics, hlir_node_t *node) {
    ir_namespace_t *current_namespace = &context->unit->root_namespace;
    if(current_module != NULL) current_namespace = &current_module->namespace;

    switch(node->type) {
        case HLIR_NODE_TYPE_ROOT: HLIR_NODE_LIST_FOREACH(&node->root.tlcs, populate_namespace_pass_two(context, current_module, current_namespace_generics, node)); break;

        case HLIR_NODE_TYPE_TLC_MODULE: {
            ir_symbol_t *symbol = ir_namespace_find_symbol_of_kind(current_namespace, node->tlc_module.name, IR_SYMBOL_KIND_MODULE);
            assert(symbol != NULL);

            namespace_generics_t *child_generics = namespace_generics_find_child(current_namespace_generics, node->tlc_module.name);
            assert(child_generics != NULL);

            HLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, populate_namespace_pass_two(context, symbol->module, child_generics, node));
            break;
        }
        case HLIR_NODE_TYPE_TLC_FUNCTION: {
            if(ir_namespace_exists_symbol(current_namespace, node->tlc_function.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_function.name);

            hlir_attribute_t *attr_link = hlir_attribute_find(&node->attributes, "link", (hlir_attribute_argument_type_t[]) { HLIR_ATTRIBUTE_ARGUMENT_TYPE_STRING }, 1);
            ir_function_t *function = alloc(sizeof(ir_function_t));
            function->name = alloc_strdup(node->tlc_extern.name);
            function->link_name = attr_link == NULL ? NULL : alloc_strdup(attr_link->arguments[0].value.string);
            function->prototype = lower_function_type(context, current_namespace, current_namespace_generics, node->tlc_function.function_type, node->source_location);
            function->is_extern = false;
            function->codegen_data = NULL;

            ir_symbol_t *symbol = ir_namespace_add_symbol(current_namespace, IR_SYMBOL_KIND_FUNCTION);
            symbol->function = function;
            break;
        }
        case HLIR_NODE_TYPE_TLC_EXTERN: {
            if(ir_namespace_exists_symbol(current_namespace, node->tlc_extern.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_extern.name);

            hlir_attribute_t *attr_link = hlir_attribute_find(&node->attributes, "link", (hlir_attribute_argument_type_t[]) { HLIR_ATTRIBUTE_ARGUMENT_TYPE_STRING }, 1);
            ir_function_t *function = alloc(sizeof(ir_function_t));
            function->name = alloc_strdup(node->tlc_extern.name);
            function->link_name = attr_link == NULL ? alloc_strdup(node->tlc_extern.name) : alloc_strdup(attr_link->arguments[0].value.string);
            function->prototype = lower_function_type(context, current_namespace, current_namespace_generics, node->tlc_extern.function_type, node->source_location);
            function->is_extern = true;
            function->codegen_data = NULL;

            ir_symbol_t *symbol = ir_namespace_add_symbol(current_namespace, IR_SYMBOL_KIND_FUNCTION);
            symbol->function = function;
            break;
        }
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION: {
            if(node->tlc_type_definition.generic_parameter_count > 0) break;

            ir_type_declaration_t *type_decl = ir_namespace_find_type(current_namespace, node->tlc_type_definition.name);
            assert(type_decl != NULL);
            *type_decl->type = *lower_type(context, current_namespace, current_namespace_generics, node->tlc_type_definition.type, node->source_location);
            break;
        }
        case HLIR_NODE_TYPE_TLC_DECLARATION:
            if(ir_namespace_exists_symbol(current_namespace, node->tlc_declaration.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_declaration.name);

            ir_variable_t *variable = alloc(sizeof(ir_variable_t));
            variable->name = alloc_strdup(node->tlc_declaration.name);
            variable->type = lower_type(context, current_namespace, current_namespace_generics, node->tlc_declaration.type, node->source_location);
            variable->is_local = false;
            variable->module = current_module;
            variable->codegen_data = NULL;

            ir_symbol_t *symbol = ir_namespace_add_symbol(current_namespace, IR_SYMBOL_KIND_VARIABLE);
            symbol->variable = variable;
            break;
        case HLIR_NODE_TYPE_TLC_ENUMERATION: break;

        HLIR_CASE_STMT();
        HLIR_CASE_EXPRESSION();
    }
}

static void populate_namespace_pass_one(lower_context_t *context, ir_module_t *current_module, namespace_generics_t *current_namespace_generics, hlir_node_t *node) {
    ir_namespace_t *current_namespace = &context->unit->root_namespace;
    if(current_module != NULL) current_namespace = &current_module->namespace;

    switch(node->type) {
        case HLIR_NODE_TYPE_ROOT: HLIR_NODE_LIST_FOREACH(&node->root.tlcs, populate_namespace_pass_one(context, current_module, current_namespace_generics, node)); break;

        case HLIR_NODE_TYPE_TLC_MODULE: {
            if(ir_namespace_exists_symbol(current_namespace, node->tlc_module.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_module.name);

            ir_module_t *module = alloc(sizeof(ir_module_t));
            module->name = alloc_strdup(node->tlc_module.name);
            module->parent = current_module;
            module->namespace = IR_NAMESPACE_INIT;

            ir_symbol_t *symbol = ir_namespace_add_symbol(current_namespace, IR_SYMBOL_KIND_MODULE);
            symbol->module = module;

            namespace_generics_t *child_generics = namespace_generics_make(context, memory_register_ptr(context->work_allocator, strdup(node->tlc_module.name)), current_namespace_generics);
            HLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, populate_namespace_pass_one(context, symbol->module, child_generics, node));
            break;
        }
        case HLIR_NODE_TYPE_TLC_FUNCTION: break;
        case HLIR_NODE_TYPE_TLC_EXTERN: break;
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION: {
            if(ir_namespace_exists_type(current_namespace, node->tlc_type_definition.name) || namespace_generics_find_generic(current_namespace_generics, node->tlc_type_definition.name) != NULL) diag_error(node->source_location, "type `%s` already exists", node->tlc_type_definition.name);
            if(node->tlc_type_definition.generic_parameter_count > 0) {
                const char **parameters = memory_allocate_array(context->work_allocator, NULL, node->tlc_type_definition.generic_parameter_count, sizeof(const char *));
                for(size_t i = 0; i < node->tlc_type_definition.generic_parameter_count; i++) {
                    parameters[i] = memory_register_ptr(context->work_allocator, strdup(node->tlc_type_definition.generic_parameters[i]));
                    if(ir_namespace_exists_type(current_namespace, parameters[i]) || namespace_generics_find_generic(current_namespace_generics, parameters[i]) != NULL) diag_error(node->source_location, "generic parameter `%s` conflicts with existing type", parameters[i]);
                }
                namespace_generics_add(context, current_namespace_generics, memory_register_ptr(context->work_allocator, strdup(node->tlc_type_definition.name)), (generic_t) {
                    .base = node->tlc_type_definition.type,
                    .parameter_count = node->tlc_type_definition.generic_parameter_count,
                    .parameters = parameters
                });
                break;
            }
            ir_namespace_add_type(current_namespace, alloc_strdup(node->tlc_type_definition.name), ir_type_cache_define(context->type_cache));
            break;
        }
        case HLIR_NODE_TYPE_TLC_DECLARATION: break;
        case HLIR_NODE_TYPE_TLC_ENUMERATION: {
            if(ir_namespace_exists_symbol(current_namespace, node->tlc_enumeration.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_enumeration.name);
            if(ir_namespace_exists_type(current_namespace, node->tlc_enumeration.name) || namespace_generics_find_generic(current_namespace_generics, node->tlc_enumeration.name) != NULL) diag_error(node->source_location, "type `%s` already exists", node->tlc_enumeration.name);

            ir_type_t *type = ir_type_cache_get_enumeration(context->type_cache, CONSTANTS_ENUM_SIZE, node->tlc_enumeration.member_count);
            ir_namespace_add_type(current_namespace, alloc_strdup(node->tlc_enumeration.name), type);

            const char **members = alloc_array(NULL, node->tlc_enumeration.member_count, sizeof(char *));
            for(size_t i = 0; i < node->tlc_enumeration.member_count; i++) members[i] = alloc_strdup(node->tlc_enumeration.members[i]);
            ir_enumeration_t *enumeration = alloc(sizeof(ir_enumeration_t));
            enumeration->name = alloc_strdup(node->tlc_enumeration.name);
            enumeration->members = members;
            enumeration->type = type;

            ir_symbol_t *symbol = ir_namespace_add_symbol(current_namespace, IR_SYMBOL_KIND_ENUMERATION);
            symbol->enumeration = enumeration;
            break;
        }

        HLIR_CASE_STMT()
        HLIR_CASE_EXPRESSION()
    }
}

lower_context_t *lower_context_make(memory_allocator_t *work_allocator, ir_unit_t *unit, ir_type_cache_t *type_cache) {
    lower_context_t *context = memory_allocate(work_allocator, sizeof(lower_context_t));
    context->work_allocator = work_allocator;
    context->unit = unit;
    context->type_cache = type_cache;
    context->namespace_generics = namespace_generics_make(context, NULL, NULL);
    return context;
}

void lower_populate_namespace(lower_context_t *context, hlir_node_t *root_node) {
    assert(root_node->type == HLIR_NODE_TYPE_ROOT);
    populate_namespace_pass_one(context, NULL, context->namespace_generics, root_node);
    populate_namespace_pass_two(context, NULL, context->namespace_generics, root_node);
}

void lower_nodes(lower_context_t *context, hlir_node_t *root_node) {
    assert(root_node->type == HLIR_NODE_TYPE_ROOT);
    lower_tlc(context, &context->unit->root_namespace, NULL, context->namespace_generics, root_node);
}