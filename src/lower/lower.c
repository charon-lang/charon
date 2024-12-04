#include "lower.h"

#include "constants.h"
#include "lib/diag.h"
#include "lib/alloc.h"
#include "llir/symbol.h"
#include "llir/type.h"

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
    llir_namespace_t *root_namespace;
    llir_type_cache_t *anon_type_cache;
    namespace_generics_t *namespace_generics;
};

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

static llir_type_t *lower_type(lower_context_t *context, llir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, hlir_type_t *type, source_location_t source_location);

static llir_type_function_t *lower_function_type(lower_context_t *context, llir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, hlir_type_function_t *function_type, source_location_t source_location) {
    llir_type_t **arguments = alloc_array(NULL, function_type->argument_count, sizeof(llir_type_t *));
    for(size_t i = 0; i < function_type->argument_count; i++) arguments[i] = lower_type(context, current_namespace, current_namespace_generics, function_type->arguments[i], source_location);
    return llir_type_cache_get_function_type(context->anon_type_cache, function_type->argument_count, arguments, function_type->varargs, lower_type(context, current_namespace, current_namespace_generics, function_type->return_type, source_location));
}

static llir_type_t *lower_type_ext(lower_context_t *context, llir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, generic_mapping_t *generic_mappings, size_t generic_mapping_count, hlir_type_t *type, source_location_t source_location) {
    if(type->is_reference) {
        if(type->reference.module_count == 0) {
            for(size_t i = 0; i < generic_mapping_count; i++) {
                if(strcmp(generic_mappings[i].name, type->reference.type_name) != 0) continue;
                return lower_type(context, current_namespace, current_namespace_generics, generic_mappings[i].type, source_location);
            }
        }

        for(size_t i = 0; i < type->reference.module_count; i++) {
            llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, type->reference.modules[i], LLIR_SYMBOL_KIND_MODULE);
            if(symbol == NULL) {
                symbol = llir_namespace_find_symbol_with_kind(context->root_namespace, type->reference.modules[i], LLIR_SYMBOL_KIND_MODULE);
                if(symbol == NULL) diag_error(source_location, "unknown module `%s`", type->reference.modules[i]);
            }
            current_namespace = symbol->module.namespace;
            current_namespace_generics = namespace_generics_find_child(current_namespace_generics, type->reference.modules[i]);
            assert(current_namespace_generics != NULL);
        }

        generic_t *generic = namespace_generics_find_generic(current_namespace_generics, type->reference.type_name);
        llir_type_t *referred_type = llir_namespace_find_type(current_namespace, type->reference.type_name);

        if(type->reference.generic_parameter_count > 0) {
            if(generic == NULL) diag_error(source_location, "unknown generic");
            if(generic->parameter_count != type->reference.generic_parameter_count) diag_error(source_location, "invalid number of parameters to generic");

            generic_mapping_t mappings[generic->parameter_count];
            for(size_t i = 0; i < generic->parameter_count; i++) mappings[i] = (generic_mapping_t) { .name = generic->parameters[i], .type = type->reference.generic_parameters[i] };
            return lower_type_ext(context, current_namespace, current_namespace_generics, mappings, generic->parameter_count, generic->base, source_location);
        } else {
            if(generic != NULL) diag_error(source_location, "type is a generic");
            if(referred_type == NULL) diag_error(source_location, "unknown type `%s`", type->reference.type_name);
            return referred_type;
        }
    }

    llir_type_t *new_type = NULL;
    switch(type->kind) {
        case HLIR_TYPE_KIND_VOID:
            new_type = llir_type_cache_get_void(context->anon_type_cache);
            break;
        case HLIR_TYPE_KIND_INTEGER:
            hlir_attribute_t *attr_allow_coerce_pointer = hlir_attribute_find(&type->attributes, "allow_coerce_pointer", NULL, 0);
            new_type = llir_type_cache_get_integer(context->anon_type_cache, type->integer.bit_size, type->integer.is_signed, attr_allow_coerce_pointer != NULL);
            break;
        case HLIR_TYPE_KIND_POINTER:
            new_type = llir_type_cache_get_pointer(context->anon_type_cache, lower_type_ext(context, current_namespace, current_namespace_generics, generic_mappings, generic_mapping_count, type->pointer.pointee, source_location));
            break;
        case HLIR_TYPE_KIND_TUPLE: {
            llir_type_t **types = alloc_array(NULL, type->tuple.type_count, sizeof(llir_type_t *));
            for(size_t i = 0; i < type->tuple.type_count; i++) types[i] = lower_type_ext(context, current_namespace, current_namespace_generics, generic_mappings, generic_mapping_count, type->tuple.types[i], source_location);
            new_type = llir_type_cache_get_tuple(context->anon_type_cache, type->tuple.type_count, types);
            break;
        }
        case HLIR_TYPE_KIND_ARRAY:
            new_type = llir_type_cache_get_array(context->anon_type_cache, lower_type_ext(context, current_namespace, current_namespace_generics, generic_mappings, generic_mapping_count, type->array.type, source_location), type->array.size);
            break;
        case HLIR_TYPE_KIND_STRUCTURE: {
            hlir_attribute_t *attr_packed = hlir_attribute_find(&type->attributes, "packed", NULL, 0);
            llir_type_structure_member_t *members = alloc_array(NULL, type->structure.member_count, sizeof(llir_type_structure_member_t));
            for(size_t i = 0; i < type->structure.member_count; i++) {
                for(size_t j = i + 1; j < type->structure.member_count; j++) if(strcmp(type->structure.members[i].name, type->structure.members[j].name) == 0) diag_error(source_location, "duplicate member `%s`", type->structure.members[i].name);
                members[i] = (llir_type_structure_member_t) {
                    .name = alloc_strdup(type->structure.members[i].name),
                    .type = lower_type_ext(context, current_namespace, current_namespace_generics, generic_mappings, generic_mapping_count, type->structure.members[i].type, source_location)
                };
                // TODO we need to confirm we arent doing recursive embedding somehow
            }
            new_type = llir_type_cache_get_structure(context->anon_type_cache, type->structure.member_count, members, attr_packed != NULL);
            break;
        }
        case HLIR_TYPE_KIND_FUNCTION_REFERENCE: {
            new_type = llir_type_cache_get_function_reference(context->anon_type_cache, lower_function_type(context, current_namespace, current_namespace_generics, type->function_reference.function_type, source_location));
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

static llir_type_t *lower_type(lower_context_t *context, llir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, hlir_type_t *type, source_location_t source_location) {
    return lower_type_ext(context, current_namespace, current_namespace_generics, NULL, 0, type, source_location);
}

static llir_node_t *lower_node(lower_context_t *context, llir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, hlir_node_t *node) {
    assert(node != NULL && (uintptr_t) node != 1);

    llir_node_t *new_node = alloc(sizeof(llir_node_t));
    new_node->source_location = node->source_location;
    new_node->next = NULL;

    switch(node->type) {
        case HLIR_NODE_TYPE_ROOT: {
            llir_node_list_t tlcs = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->root.tlcs, {
                llir_node_t *lowered_node = lower_node(context, current_namespace, current_namespace_generics, node);
                if(lowered_node == NULL) continue;
                llir_node_list_append(&tlcs, lowered_node);
            });

            new_node->type = LLIR_NODE_TYPE_ROOT;
            new_node->root.tlcs = tlcs;
            break;
        }

        case HLIR_NODE_TYPE_TLC_MODULE: {
            llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_module.name, LLIR_SYMBOL_KIND_MODULE);
            assert(symbol != NULL);

            namespace_generics_t *child_generics = namespace_generics_find_child(current_namespace_generics, node->tlc_module.name);
            assert(child_generics != NULL);

            llir_node_list_t tlcs = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, {
                llir_node_t *lowered_node = lower_node(context, symbol->module.namespace, child_generics, node);
                if(lowered_node == NULL) continue;
                llir_node_list_append(&tlcs, lowered_node);
            });

            new_node->type = LLIR_NODE_TYPE_TLC_MODULE;
            new_node->tlc_module.symbol = symbol;
            new_node->tlc_module.tlcs = tlcs;
            break;
        }
        case HLIR_NODE_TYPE_TLC_FUNCTION: {
            llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_function.name, LLIR_SYMBOL_KIND_FUNCTION);
            assert(symbol != NULL);

            const char **arguments = alloc_array(NULL, node->tlc_function.function_type->argument_count, sizeof(char *));
            for(size_t i = 0; i < node->tlc_function.function_type->argument_count; i++) arguments[i] = alloc_strdup(node->tlc_function.argument_names[i]);

            new_node->type = LLIR_NODE_TYPE_TLC_FUNCTION;
            new_node->tlc_function.symbol = symbol;
            new_node->tlc_function.argument_names = arguments;
            new_node->tlc_function.statement = node->tlc_function.statement == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->tlc_function.statement);
            new_node->tlc_function.function_type = symbol->function.function_type;
            break;
        }
        case HLIR_NODE_TYPE_TLC_DECLARATION: {
            llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_declaration.name, LLIR_SYMBOL_KIND_VARIABLE);
            assert(symbol != NULL);

            new_node->type = LLIR_NODE_TYPE_TLC_DECLARATION;
            new_node->tlc_declaration.symbol = symbol;
            new_node->tlc_declaration.initial = node->tlc_declaration.initial == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->tlc_declaration.initial);
            break;
        }
        case HLIR_NODE_TYPE_TLC_EXTERN:
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION:
        case HLIR_NODE_TYPE_TLC_ENUMERATION:
            alloc_free(new_node);
            new_node = NULL;
            break;

        case HLIR_NODE_TYPE_STMT_BLOCK: {
            llir_node_list_t statements = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->stmt_block.statements, llir_node_list_append(&statements, lower_node(context, current_namespace, current_namespace_generics, node)));

            new_node->type = LLIR_NODE_TYPE_STMT_BLOCK;
            new_node->stmt_block.statements = statements;
            break;
        }
        case HLIR_NODE_TYPE_STMT_DECLARATION: {
            new_node->type = LLIR_NODE_TYPE_STMT_DECLARATION;
            new_node->stmt_declaration.name = alloc_strdup(node->stmt_declaration.name);
            new_node->stmt_declaration.type = node->stmt_declaration.type == NULL ? NULL : lower_type(context, current_namespace, current_namespace_generics, node->stmt_declaration.type, node->source_location);
            new_node->stmt_declaration.initial = node->stmt_declaration.initial == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_declaration.initial);
            break;
        }
        case HLIR_NODE_TYPE_STMT_EXPRESSION: {
            new_node->type = LLIR_NODE_TYPE_STMT_EXPRESSION;
            new_node->stmt_expression.expression = lower_node(context, current_namespace, current_namespace_generics, node->stmt_expression.expression);
            break;
        }
        case HLIR_NODE_TYPE_STMT_RETURN: {
            new_node->type = LLIR_NODE_TYPE_STMT_RETURN;
            new_node->stmt_return.value = node->stmt_return.value == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_return.value);
            break;
        }
        case HLIR_NODE_TYPE_STMT_IF: {
            new_node->type = LLIR_NODE_TYPE_STMT_IF;
            new_node->stmt_if.condition = lower_node(context, current_namespace, current_namespace_generics, node->stmt_if.condition);
            new_node->stmt_if.body = node->stmt_if.body == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_if.body);
            new_node->stmt_if.else_body = node->stmt_if.else_body == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_if.else_body);
            break;
        }
        case HLIR_NODE_TYPE_STMT_WHILE: {
            new_node->type = LLIR_NODE_TYPE_STMT_WHILE;
            new_node->stmt_while.condition = node->stmt_while.condition == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_while.condition);
            new_node->stmt_while.body = node->stmt_while.body == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_while.body);
            break;
        }
        case HLIR_NODE_TYPE_STMT_CONTINUE: {
            new_node->type = LLIR_NODE_TYPE_STMT_CONTINUE;
            break;
        }
        case HLIR_NODE_TYPE_STMT_BREAK: {
            new_node->type = LLIR_NODE_TYPE_STMT_BREAK;
            break;
        }
        case HLIR_NODE_TYPE_STMT_FOR: {
            new_node->type = LLIR_NODE_TYPE_STMT_FOR;
            new_node->stmt_for.declaration = node->stmt_for.declaration == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_for.declaration);
            new_node->stmt_for.condition = node->stmt_for.condition == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_for.condition);
            new_node->stmt_for.expr_after = node->stmt_for.expr_after == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_for.expr_after);
            new_node->stmt_for.body = node->stmt_for.body == NULL ? NULL : lower_node(context, current_namespace, current_namespace_generics, node->stmt_for.body);
            break;
        }

        case HLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC: {
            new_node->type = LLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC;
            new_node->expr.is_const = true;
            new_node->expr.literal.numeric_value = node->expr_literal.numeric_value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_LITERAL_STRING: {
            new_node->type = LLIR_NODE_TYPE_EXPR_LITERAL_STRING;
            new_node->expr.is_const = true;
            new_node->expr.literal.string_value = alloc_strdup(node->expr_literal.string_value);
            break;
        }
        case HLIR_NODE_TYPE_EXPR_LITERAL_CHAR: {
            new_node->type = LLIR_NODE_TYPE_EXPR_LITERAL_CHAR;
            new_node->expr.is_const = true;
            new_node->expr.literal.char_value = node->expr_literal.char_value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_LITERAL_BOOL: {
            new_node->type = LLIR_NODE_TYPE_EXPR_LITERAL_BOOL;
            new_node->expr.is_const = true;
            new_node->expr.literal.bool_value = node->expr_literal.bool_value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_BINARY: {
            llir_node_binary_operation_t operation;
            switch(node->expr_binary.operation) {
                case HLIR_NODE_BINARY_OPERATION_ASSIGN: operation = LLIR_NODE_BINARY_OPERATION_ASSIGN; break;
                case HLIR_NODE_BINARY_OPERATION_ADDITION: operation = LLIR_NODE_BINARY_OPERATION_ADDITION; break;
                case HLIR_NODE_BINARY_OPERATION_SUBTRACTION: operation = LLIR_NODE_BINARY_OPERATION_SUBTRACTION; break;
                case HLIR_NODE_BINARY_OPERATION_MULTIPLICATION: operation = LLIR_NODE_BINARY_OPERATION_MULTIPLICATION; break;
                case HLIR_NODE_BINARY_OPERATION_DIVISION: operation = LLIR_NODE_BINARY_OPERATION_DIVISION; break;
                case HLIR_NODE_BINARY_OPERATION_MODULO: operation = LLIR_NODE_BINARY_OPERATION_MODULO; break;
                case HLIR_NODE_BINARY_OPERATION_GREATER: operation = LLIR_NODE_BINARY_OPERATION_GREATER; break;
                case HLIR_NODE_BINARY_OPERATION_GREATER_EQUAL: operation = LLIR_NODE_BINARY_OPERATION_GREATER_EQUAL; break;
                case HLIR_NODE_BINARY_OPERATION_LESS: operation = LLIR_NODE_BINARY_OPERATION_LESS; break;
                case HLIR_NODE_BINARY_OPERATION_LESS_EQUAL: operation = LLIR_NODE_BINARY_OPERATION_LESS_EQUAL; break;
                case HLIR_NODE_BINARY_OPERATION_EQUAL: operation = LLIR_NODE_BINARY_OPERATION_EQUAL; break;
                case HLIR_NODE_BINARY_OPERATION_NOT_EQUAL: operation = LLIR_NODE_BINARY_OPERATION_NOT_EQUAL; break;
                case HLIR_NODE_BINARY_OPERATION_LOGICAL_AND: operation = LLIR_NODE_BINARY_OPERATION_LOGICAL_AND; break;
                case HLIR_NODE_BINARY_OPERATION_LOGICAL_OR: operation = LLIR_NODE_BINARY_OPERATION_LOGICAL_OR; break;
                case HLIR_NODE_BINARY_OPERATION_SHIFT_LEFT: operation = LLIR_NODE_BINARY_OPERATION_SHIFT_LEFT; break;
                case HLIR_NODE_BINARY_OPERATION_SHIFT_RIGHT: operation = LLIR_NODE_BINARY_OPERATION_SHIFT_RIGHT; break;
                case HLIR_NODE_BINARY_OPERATION_AND: operation = LLIR_NODE_BINARY_OPERATION_AND; break;
                case HLIR_NODE_BINARY_OPERATION_OR: operation = LLIR_NODE_BINARY_OPERATION_OR; break;
                case HLIR_NODE_BINARY_OPERATION_XOR: operation = LLIR_NODE_BINARY_OPERATION_XOR; break;
            }

            llir_node_t *left = lower_node(context, current_namespace, current_namespace_generics, node->expr_binary.left);
            llir_node_t *right = lower_node(context, current_namespace, current_namespace_generics, node->expr_binary.right);

            new_node->type = LLIR_NODE_TYPE_EXPR_BINARY;
            new_node->expr.is_const = left->expr.is_const && right->expr.is_const;
            new_node->expr.binary.operation = operation;
            new_node->expr.binary.left = left;
            new_node->expr.binary.right = right;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_UNARY: {
            llir_node_unary_operation_t operation;
            switch(node->expr_unary.operation) {
                case HLIR_NODE_UNARY_OPERATION_NOT: operation = LLIR_NODE_UNARY_OPERATION_NOT; break;
                case HLIR_NODE_UNARY_OPERATION_NEGATIVE: operation = LLIR_NODE_UNARY_OPERATION_NEGATIVE; break;
                case HLIR_NODE_UNARY_OPERATION_DEREF: operation = LLIR_NODE_UNARY_OPERATION_DEREF; break;
                case HLIR_NODE_UNARY_OPERATION_REF: operation = LLIR_NODE_UNARY_OPERATION_REF; break;
            }

            llir_node_t *operand = lower_node(context, current_namespace, current_namespace_generics, node->expr_unary.operand);

            new_node->type = LLIR_NODE_TYPE_EXPR_UNARY;
            new_node->expr.is_const = operand->expr.is_const;
            new_node->expr.unary.operation = operation;
            new_node->expr.unary.operand = operand;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_VARIABLE: {
            new_node->type = LLIR_NODE_TYPE_EXPR_VARIABLE;
            new_node->expr.is_const = false; // TODO: if we can resolve const variables here... could be
            new_node->expr.variable.name = alloc_strdup(node->expr_variable.name);
            break;
        }
        case HLIR_NODE_TYPE_EXPR_TUPLE: {
            llir_node_list_t values = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->expr_tuple.values, llir_node_list_append(&values, lower_node(context, current_namespace, current_namespace_generics, node)));

            new_node->type = LLIR_NODE_TYPE_EXPR_TUPLE;
            new_node->expr.is_const = true;
            LLIR_NODE_LIST_FOREACH(&values, new_node->expr.is_const = new_node->expr.is_const && node->expr.is_const);
            new_node->expr.tuple.values = values;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_CALL: {
            llir_node_list_t arguments = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->expr_call.arguments, llir_node_list_append(&arguments, lower_node(context, current_namespace, current_namespace_generics, node)));

            new_node->type = LLIR_NODE_TYPE_EXPR_CALL;
            new_node->expr.is_const = false;
            new_node->expr.call.function_reference = lower_node(context, current_namespace, current_namespace_generics, node->expr_call.function_reference);
            new_node->expr.call.arguments = arguments;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_CAST: {
            llir_node_t *value = lower_node(context, current_namespace, current_namespace_generics, node->expr_cast.value);

            new_node->type = LLIR_NODE_TYPE_EXPR_CAST;
            new_node->expr.is_const = value->expr.is_const;
            new_node->expr.cast.type = lower_type(context, current_namespace, current_namespace_generics, node->expr_cast.type, node->source_location);
            new_node->expr.cast.value = value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_SUBSCRIPT: {
            llir_node_t *value = lower_node(context, current_namespace, current_namespace_generics, node->expr_subscript.value);

            new_node->type = LLIR_NODE_TYPE_EXPR_SUBSCRIPT;
            switch(node->expr_subscript.type) {
                case HLIR_NODE_SUBSCRIPT_TYPE_INDEX:
                    new_node->expr.is_const = false;
                    new_node->expr.subscript.type = LLIR_NODE_SUBSCRIPT_TYPE_INDEX;
                    new_node->expr.subscript.index = lower_node(context, current_namespace, current_namespace_generics, node->expr_subscript.index);
                    break;
                case HLIR_NODE_SUBSCRIPT_TYPE_INDEX_CONST:
                    new_node->expr.is_const = value->expr.is_const;
                    new_node->expr.subscript.type = LLIR_NODE_SUBSCRIPT_TYPE_INDEX_CONST;
                    new_node->expr.subscript.index_const = node->expr_subscript.index_const;
                    break;
                case HLIR_NODE_SUBSCRIPT_TYPE_MEMBER:
                    new_node->expr.is_const = value->expr.is_const;
                    new_node->expr.subscript.type = LLIR_NODE_SUBSCRIPT_TYPE_MEMBER;
                    new_node->expr.subscript.member = alloc_strdup(node->expr_subscript.member);
                    break;
            }
            new_node->expr.subscript.value = value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_SELECTOR: {
            llir_node_t *value = lower_node(context, current_namespace, current_namespace_generics, node->expr_selector.value);

            new_node->type = LLIR_NODE_TYPE_EXPR_SELECTOR;
            new_node->expr.is_const = value->expr.is_const;
            new_node->expr.selector.name = alloc_strdup(node->expr_selector.name);
            new_node->expr.selector.value = value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_SIZEOF: {
            new_node->type = LLIR_NODE_TYPE_EXPR_SIZEOF;
            new_node->expr.is_const = true;
            new_node->expr._sizeof.type = lower_type(context, current_namespace, current_namespace_generics, node->expr_sizeof.type, node->source_location);
            break;
        }
    }
    for(size_t i = 0; i < node->attributes.attribute_count; i++) {
        if(node->attributes.attributes[i].consumed) continue;
        diag_warn(node->attributes.attributes[i].source_location, "Unhandled attribute");
    }
    return new_node;
}

static void populate_namespace_pass_two(lower_context_t *context, llir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, hlir_node_t *node) {
    switch(node->type) {
        case HLIR_NODE_TYPE_ROOT: HLIR_NODE_LIST_FOREACH(&node->root.tlcs, populate_namespace_pass_two(context, current_namespace, current_namespace_generics, node)); break;

        case HLIR_NODE_TYPE_TLC_MODULE:
            llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_module.name, LLIR_SYMBOL_KIND_MODULE);
            assert(symbol != NULL);

            namespace_generics_t *child_generics = namespace_generics_find_child(current_namespace_generics, node->tlc_module.name);
            assert(child_generics != NULL);

            HLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, populate_namespace_pass_two(context, symbol->module.namespace, child_generics, node));
            break;
        case HLIR_NODE_TYPE_TLC_FUNCTION: {
            if(llir_namespace_exists_symbol(current_namespace, node->tlc_function.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_function.name);
            hlir_attribute_t *attr_link = hlir_attribute_find(&node->attributes, "link", (hlir_attribute_argument_type_t[]) { HLIR_ATTRIBUTE_ARGUMENT_TYPE_STRING }, 1);
            llir_namespace_add_symbol_function(current_namespace, alloc_strdup(node->tlc_function.name), attr_link == NULL ? NULL : alloc_strdup(attr_link->arguments[0].value.string), lower_function_type(context, current_namespace, current_namespace_generics, node->tlc_function.function_type, node->source_location));
            break;
        }
        case HLIR_NODE_TYPE_TLC_EXTERN: {
            if(llir_namespace_exists_symbol(current_namespace, node->tlc_extern.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_extern.name);
            hlir_attribute_t *attr_link = hlir_attribute_find(&node->attributes, "link", (hlir_attribute_argument_type_t[]) { HLIR_ATTRIBUTE_ARGUMENT_TYPE_STRING }, 1);
            llir_namespace_add_symbol_function(current_namespace, alloc_strdup(node->tlc_extern.name), attr_link == NULL ? alloc_strdup(node->tlc_extern.name) : alloc_strdup(attr_link->arguments[0].value.string), lower_function_type(context, current_namespace, current_namespace_generics, node->tlc_extern.function_type, node->source_location));
            break;
        }
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION:
            if(node->tlc_type_definition.generic_parameter_count > 0) break;
            llir_type_t *lowered_type = lower_type(context, current_namespace, current_namespace_generics, node->tlc_type_definition.type, node->source_location);
            llir_namespace_update_type(current_namespace, node->tlc_type_definition.name, *lowered_type);
            break;
        case HLIR_NODE_TYPE_TLC_DECLARATION:
            if(llir_namespace_exists_symbol(current_namespace, node->tlc_declaration.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_declaration.name);
            llir_namespace_add_symbol_variable(current_namespace, alloc_strdup(node->tlc_declaration.name), lower_type(context, current_namespace, current_namespace_generics, node->tlc_declaration.type, node->source_location));
            break;
        case HLIR_NODE_TYPE_TLC_ENUMERATION: break;

        HLIR_CASE_STMT();
        HLIR_CASE_EXPRESSION();
    }
}

static void populate_namespace_pass_one(lower_context_t *context, llir_namespace_t *current_namespace, namespace_generics_t *current_namespace_generics, hlir_node_t *node) {
    switch(node->type) {
        case HLIR_NODE_TYPE_ROOT: HLIR_NODE_LIST_FOREACH(&node->root.tlcs, populate_namespace_pass_one(context, current_namespace, current_namespace_generics, node)); break;

        case HLIR_NODE_TYPE_TLC_MODULE:
            if(llir_namespace_exists_symbol(current_namespace, node->tlc_module.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_module.name);
            llir_symbol_t *symbol = llir_namespace_add_symbol_module(current_namespace, alloc_strdup(node->tlc_module.name));
            namespace_generics_t *child_generics = namespace_generics_make(context, memory_register_ptr(context->work_allocator, strdup(node->tlc_module.name)), current_namespace_generics);
            HLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, populate_namespace_pass_one(context, symbol->module.namespace, child_generics, node));
            break;
        case HLIR_NODE_TYPE_TLC_FUNCTION: break;
        case HLIR_NODE_TYPE_TLC_EXTERN: break;
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION:
            if(llir_namespace_exists_type(current_namespace, node->tlc_type_definition.name) || namespace_generics_find_generic(current_namespace_generics, node->tlc_type_definition.name) != NULL) diag_error(node->source_location, "type `%s` already exists", node->tlc_type_definition.name);
            if(node->tlc_type_definition.generic_parameter_count > 0) {
                const char **parameters = memory_allocate_array(context->work_allocator, NULL, node->tlc_type_definition.generic_parameter_count, sizeof(const char *));
                for(size_t i = 0; i < node->tlc_type_definition.generic_parameter_count; i++) {
                    parameters[i] = memory_register_ptr(context->work_allocator, strdup(node->tlc_type_definition.generic_parameters[i]));
                    if(llir_namespace_exists_type(current_namespace, parameters[i]) || namespace_generics_find_generic(current_namespace_generics, parameters[i]) != NULL) diag_error(node->source_location, "generic parameter `%s` conflicts with existing type", parameters[i]);
                }
                namespace_generics_add(context, current_namespace_generics, memory_register_ptr(context->work_allocator, strdup(node->tlc_type_definition.name)), (generic_t) {
                    .base = node->tlc_type_definition.type,
                    .parameter_count = node->tlc_type_definition.generic_parameter_count,
                    .parameters = parameters
                });
                break;
            }
            llir_namespace_add_type(current_namespace, alloc_strdup(node->tlc_type_definition.name));
            break;
        case HLIR_NODE_TYPE_TLC_DECLARATION: break;
        case HLIR_NODE_TYPE_TLC_ENUMERATION:
            if(llir_namespace_exists_symbol(current_namespace, node->tlc_enumeration.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_enumeration.name);
            if(llir_namespace_exists_type(current_namespace, node->tlc_enumeration.name) || namespace_generics_find_generic(current_namespace_generics, node->tlc_enumeration.name) != NULL) diag_error(node->source_location, "type `%s` already exists", node->tlc_enumeration.name);
            llir_namespace_add_type(current_namespace, alloc_strdup(node->tlc_enumeration.name));
            llir_type_t *type = llir_namespace_update_type(current_namespace, node->tlc_enumeration.name, *llir_type_cache_get_enumeration(context->anon_type_cache, CONSTANTS_ENUM_SIZE, node->tlc_enumeration.member_count));
            const char **members = alloc_array(NULL, node->tlc_enumeration.member_count, sizeof(char *));
            for(size_t i = 0; i < node->tlc_enumeration.member_count; i++) members[i] = alloc_strdup(node->tlc_enumeration.members[i]);
            llir_namespace_add_symbol_enumeration(current_namespace, alloc_strdup(node->tlc_enumeration.name), type, members);
            break;

        HLIR_CASE_STMT()
        HLIR_CASE_EXPRESSION()
    }
}

lower_context_t *lower_context_make(memory_allocator_t *work_allocator, llir_namespace_t *root_namespace, llir_type_cache_t *anon_type_cache) {
    lower_context_t *context = memory_allocate(work_allocator, sizeof(lower_context_t));
    context->work_allocator = work_allocator;
    context->root_namespace = root_namespace;
    context->anon_type_cache = anon_type_cache;
    context->namespace_generics = namespace_generics_make(context, NULL, NULL);
    return context;
}

void lower_populate_namespace(lower_context_t *context, hlir_node_t *root_node) {
    assert(root_node->type == HLIR_NODE_TYPE_ROOT);
    populate_namespace_pass_one(context, context->root_namespace, context->namespace_generics, root_node);
    populate_namespace_pass_two(context, context->root_namespace, context->namespace_generics, root_node);
}

llir_node_t *lower_nodes(lower_context_t *context, hlir_node_t *root_node) {
    assert(root_node->type == HLIR_NODE_TYPE_ROOT);
    return lower_node(context, context->root_namespace, context->namespace_generics, root_node);
}