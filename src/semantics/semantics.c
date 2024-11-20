#include "semantics.h"

#include "lib/diag.h"
#include "llir/symbol.h"
#include "llir/type.h"

#include <assert.h>
#include <string.h>
#include <malloc.h>

typedef struct {
    llir_namespace_t *root_namespace;
    llir_type_cache_t *anon_type_cache;
} context_t;

static llir_type_t *lower_type(context_t *context, llir_namespace_t *current_namespace, hlir_type_t *type, source_location_t source_location) {
    if(type->is_reference) {
        llir_type_t *referred_type = llir_namespace_find_type(current_namespace, type->reference.type_name);
        if(referred_type == NULL) diag_error(source_location, "unknown type `%s`", type->reference.type_name);
        return referred_type;
    }

    llir_type_t *new_type = NULL;
    switch(type->kind) {
        case HLIR_TYPE_KIND_VOID:
            new_type = llir_type_cache_get_void(context->anon_type_cache);
            break;
        case HLIR_TYPE_KIND_INTEGER:
            new_type = llir_type_cache_get_integer(context->anon_type_cache, type->integer.bit_size, type->integer.is_signed);
            break;
        case HLIR_TYPE_KIND_POINTER:
            new_type = llir_type_cache_get_pointer(context->anon_type_cache, lower_type(context, current_namespace, type->pointer.pointee, source_location));
            break;
        case HLIR_TYPE_KIND_TUPLE: {
            llir_type_t **types = malloc(type->tuple.type_count * sizeof(llir_type_t *));
            for(size_t i = 0; i < type->tuple.type_count; i++) types[i] = lower_type(context, current_namespace, type->tuple.types[i], source_location);
            new_type = llir_type_cache_get_tuple(context->anon_type_cache, type->tuple.type_count, types);
            break;
        }
        case HLIR_TYPE_KIND_ARRAY:
            new_type = llir_type_cache_get_array(context->anon_type_cache, lower_type(context, current_namespace, type->array.type, source_location), type->array.size);
            break;
        case HLIR_TYPE_KIND_STRUCTURE: {
            hlir_attribute_t *attr_packed = hlir_attribute_find(&type->attributes, HLIR_ATTRIBUTE_KIND_PACKED);
            if(attr_packed != NULL) attr_packed->consumed = true;

            llir_type_structure_member_t *members = malloc(type->structure.member_count * sizeof(llir_type_structure_member_t));
            for(size_t i = 0; i < type->structure.member_count; i++) {
                for(size_t j = i + 1; j < type->structure.member_count; j++) if(strcmp(type->structure.members[i].name, type->structure.members[j].name) == 0) diag_error(source_location, "duplicate member `%s`", type->structure.members[i].name);
                members[i] = (llir_type_structure_member_t) {
                    .name = type->structure.members[i].name,
                    .type = lower_type(context, current_namespace, type->structure.members[i].type, source_location)
                };
                // TODO we need to confirm we arent doing recursive embedding somehow
            }
            new_type = llir_type_cache_get_structure(context->anon_type_cache, type->structure.member_count, members, attr_packed != NULL);
            break;
        }
        case HLIR_TYPE_KIND_FUNCTION: {
            llir_type_t **arguments = malloc(type->function.argument_count * sizeof(llir_type_t *));
            for(size_t i = 0; i < type->function.argument_count; i++) arguments[i] = lower_type(context, current_namespace, type->function.arguments[i], source_location);
            new_type = llir_type_cache_get_function(context->anon_type_cache, type->function.argument_count, arguments, type->function.varargs, lower_type(context, current_namespace, type->function.return_type, source_location));
            break;
        }
    }
    assert(new_type != NULL);
    for(size_t i = 0; i < type->attributes.attribute_count; i++) {
        if(type->attributes.attributes[i].consumed) continue;
        diag_warn(type->attributes.attributes[i].source_location, "Unhandled attribute");
    }
    return new_type;
}

static llir_node_t *lower_node(context_t *context, llir_namespace_t *current_namespace, hlir_node_t *node) {
    assert(node != NULL && (uintptr_t) node != 1);

    llir_node_t *new_node = malloc(sizeof(llir_node_t));
    new_node->source_location = node->source_location;
    new_node->next = NULL;

    switch(node->type) {
        case HLIR_NODE_TYPE_ROOT: {
            llir_node_list_t tlcs = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->root.tlcs, {
                llir_node_t *lowered_node = lower_node(context, current_namespace, node);
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

            llir_node_list_t tlcs = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, llir_node_list_append(&tlcs, lower_node(context, symbol->module.namespace, node)));

            new_node->type = LLIR_NODE_TYPE_TLC_MODULE;
            new_node->tlc_module.name = node->tlc_module.name;
            new_node->tlc_module.tlcs = tlcs;
            break;
        }
        case HLIR_NODE_TYPE_TLC_FUNCTION: {
            llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_function.name, LLIR_SYMBOL_KIND_FUNCTION);
            assert(symbol != NULL);

            new_node->type = LLIR_NODE_TYPE_TLC_FUNCTION;
            new_node->tlc_function.name = node->tlc_function.name;
            new_node->tlc_function.argument_names = node->tlc_function.argument_names;
            new_node->tlc_function.statement = node->tlc_function.statement == NULL ? NULL : lower_node(context, current_namespace, node->tlc_function.statement);
            new_node->tlc_function.type = symbol->function.type;
            break;
        }
        case HLIR_NODE_TYPE_TLC_EXTERN:
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION:
        case HLIR_NODE_TYPE_TLC_DECLARATION:
            free(new_node);
            new_node = NULL;
            break;

        case HLIR_NODE_TYPE_STMT_BLOCK: {
            llir_node_list_t statements = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->stmt_block.statements, llir_node_list_append(&statements, lower_node(context, current_namespace, node)));

            new_node->type = LLIR_NODE_TYPE_STMT_BLOCK;
            new_node->stmt_block.statements = statements;
            break;
        }
        case HLIR_NODE_TYPE_STMT_DECLARATION: {
            new_node->type = LLIR_NODE_TYPE_STMT_DECLARATION;
            new_node->stmt_declaration.name = node->stmt_declaration.name;
            new_node->stmt_declaration.type = node->stmt_declaration.type == NULL ? NULL : lower_type(context, current_namespace, node->stmt_declaration.type, node->source_location);
            new_node->stmt_declaration.initial = node->stmt_declaration.initial == NULL ? NULL : lower_node(context, current_namespace, node->stmt_declaration.initial);
            break;
        }
        case HLIR_NODE_TYPE_STMT_EXPRESSION: {
            new_node->type = LLIR_NODE_TYPE_STMT_EXPRESSION;
            new_node->stmt_expression.expression = lower_node(context, current_namespace, node->stmt_expression.expression);
            break;
        }
        case HLIR_NODE_TYPE_STMT_RETURN: {
            new_node->type = LLIR_NODE_TYPE_STMT_RETURN;
            new_node->stmt_return.value = node->stmt_return.value == NULL ? NULL : lower_node(context, current_namespace, node->stmt_return.value);
            break;
        }
        case HLIR_NODE_TYPE_STMT_IF: {
            new_node->type = LLIR_NODE_TYPE_STMT_IF;
            new_node->stmt_if.condition = lower_node(context, current_namespace, node->stmt_if.condition);
            new_node->stmt_if.body = node->stmt_if.body == NULL ? NULL : lower_node(context, current_namespace, node->stmt_if.body);
            new_node->stmt_if.else_body = node->stmt_if.else_body == NULL ? NULL : lower_node(context, current_namespace, node->stmt_if.else_body);
            break;
        }
        case HLIR_NODE_TYPE_STMT_WHILE: {
            new_node->type = LLIR_NODE_TYPE_STMT_WHILE;
            new_node->stmt_while.condition = node->stmt_while.condition == NULL ? NULL : lower_node(context, current_namespace, node->stmt_while.condition);
            new_node->stmt_while.body = node->stmt_while.body == NULL ? NULL : lower_node(context, current_namespace, node->stmt_while.body);
            break;
        }

        case HLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC: {
            new_node->type = LLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC;
            new_node->expr.literal.numeric_value = node->expr_literal.numeric_value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_LITERAL_STRING: {
            new_node->type = LLIR_NODE_TYPE_EXPR_LITERAL_STRING;
            new_node->expr.literal.string_value = node->expr_literal.string_value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_LITERAL_CHAR: {
            new_node->type = LLIR_NODE_TYPE_EXPR_LITERAL_CHAR;
            new_node->expr.literal.char_value = node->expr_literal.char_value;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_LITERAL_BOOL: {
            new_node->type = LLIR_NODE_TYPE_EXPR_LITERAL_BOOL;
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
                case HLIR_NODE_BINARY_OPERATION_SHIFT_LEFT: operation = LLIR_NODE_BINARY_OPERATION_SHIFT_LEFT; break;
                case HLIR_NODE_BINARY_OPERATION_SHIFT_RIGHT: operation = LLIR_NODE_BINARY_OPERATION_SHIFT_RIGHT; break;
            }

            new_node->type = LLIR_NODE_TYPE_EXPR_BINARY;
            new_node->expr.binary.operation = operation;
            new_node->expr.binary.left = lower_node(context, current_namespace, node->expr_binary.left);
            new_node->expr.binary.right = lower_node(context, current_namespace, node->expr_binary.right);
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

            new_node->type = LLIR_NODE_TYPE_EXPR_UNARY;
            new_node->expr.unary.operation = operation;
            new_node->expr.unary.operand = lower_node(context, current_namespace, node->expr_unary.operand);
            break;
        }
        case HLIR_NODE_TYPE_EXPR_VARIABLE: {
            new_node->type = LLIR_NODE_TYPE_EXPR_VARIABLE;
            new_node->expr.variable.name = node->expr_variable.name;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_TUPLE: {
            llir_node_list_t values = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->expr_tuple.values, llir_node_list_append(&values, lower_node(context, current_namespace, node)));

            new_node->type = LLIR_NODE_TYPE_EXPR_TUPLE;
            new_node->expr.tuple.values = values;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_CALL: {
            llir_node_list_t arguments = LLIR_NODE_LIST_INIT;
            HLIR_NODE_LIST_FOREACH(&node->expr_call.arguments, llir_node_list_append(&arguments, lower_node(context, current_namespace, node)));

            new_node->type = LLIR_NODE_TYPE_EXPR_CALL;
            new_node->expr.call.function_reference = lower_node(context, current_namespace, node->expr_call.function_reference);
            new_node->expr.call.arguments = arguments;
            break;
        }
        case HLIR_NODE_TYPE_EXPR_CAST: {
            new_node->type = LLIR_NODE_TYPE_EXPR_CAST;
            new_node->expr.cast.type = lower_type(context, current_namespace, node->expr_cast.type, node->source_location);
            new_node->expr.cast.value = lower_node(context, current_namespace, node->expr_cast.value);
            break;
        }
        case HLIR_NODE_TYPE_EXPR_SUBSCRIPT: {
            new_node->type = LLIR_NODE_TYPE_EXPR_SUBSCRIPT;
            new_node->expr.subscript.value = lower_node(context, current_namespace, node->expr_subscript.value);
            switch(node->expr_subscript.type) {
                case HLIR_NODE_SUBSCRIPT_TYPE_INDEX:
                    new_node->expr.subscript.type = LLIR_NODE_SUBSCRIPT_TYPE_INDEX;
                    new_node->expr.subscript.index = lower_node(context, current_namespace, node->expr_subscript.index);
                    break;
                case HLIR_NODE_SUBSCRIPT_TYPE_INDEX_CONST:
                    new_node->expr.subscript.type = LLIR_NODE_SUBSCRIPT_TYPE_INDEX_CONST;
                    new_node->expr.subscript.index_const = node->expr_subscript.index_const;
                    break;
                case HLIR_NODE_SUBSCRIPT_TYPE_MEMBER:
                    new_node->expr.subscript.type = LLIR_NODE_SUBSCRIPT_TYPE_MEMBER;
                    new_node->expr.subscript.member = node->expr_subscript.member;
                    break;
            }
            break;
        }
        case HLIR_NODE_TYPE_EXPR_SELECTOR: {
            new_node->type = LLIR_NODE_TYPE_EXPR_SELECTOR;
            new_node->expr.selector.name = node->expr_selector.name;
            new_node->expr.selector.value = lower_node(context, current_namespace, node->expr_selector.value);
            break;
        }
    }
    for(size_t i = 0; i < node->attributes.attribute_count; i++) {
        if(node->attributes.attributes[i].consumed) continue;
        diag_warn(node->attributes.attributes[i].source_location, "Unhandled attribute");
    }
    return new_node;
}

static void pass_two(context_t *context, llir_namespace_t *current_namespace, hlir_node_t *node) {
    switch(node->type) {
        case HLIR_NODE_TYPE_ROOT: HLIR_NODE_LIST_FOREACH(&node->root.tlcs, pass_two(context, current_namespace, node)); break;

        case HLIR_NODE_TYPE_TLC_MODULE:
            llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_module.name, LLIR_SYMBOL_KIND_MODULE);
            assert(symbol != NULL);
            HLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, pass_two(context, symbol->module.namespace, node));
            break;
        case HLIR_NODE_TYPE_TLC_FUNCTION:
            if(llir_namespace_exists_symbol(current_namespace, node->tlc_function.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_function.name);
            llir_namespace_add_symbol_function(current_namespace, node->tlc_function.name, lower_type(context, current_namespace, node->tlc_function.type, node->source_location));
            break;
        case HLIR_NODE_TYPE_TLC_EXTERN:
            if(current_namespace != context->root_namespace) diag_error(node->source_location, "extern can only be declared in the root");
            if(llir_namespace_exists_symbol(current_namespace, node->tlc_extern.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_extern.name);
            llir_namespace_add_symbol_function(current_namespace, node->tlc_extern.name, lower_type(context, current_namespace, node->tlc_extern.type, node->source_location));
            break;
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION:
            llir_type_t *lowered_type = lower_type(context, current_namespace, node->tlc_type_definition.type, node->source_location);
            llir_namespace_update_type(current_namespace, node->tlc_type_definition.name, *lowered_type);
            free(lowered_type);
            break;
        case HLIR_NODE_TYPE_TLC_DECLARATION:
            if(llir_namespace_exists_symbol(current_namespace, node->tlc_declaration.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_declaration.name);
            llir_namespace_add_symbol_variable(current_namespace, node->tlc_declaration.name, lower_type(context, current_namespace, node->tlc_declaration.type, node->source_location));
            break;

        HLIR_CASE_STMT();
        HLIR_CASE_EXPRESSION();
    }
}

static void pass_one(context_t *context, llir_namespace_t *current_namespace, hlir_node_t *node) {
    switch(node->type) {
        case HLIR_NODE_TYPE_ROOT: HLIR_NODE_LIST_FOREACH(&node->root.tlcs, pass_one(context, current_namespace, node)); break;

        case HLIR_NODE_TYPE_TLC_MODULE:
            if(llir_namespace_exists_symbol(current_namespace, node->tlc_module.name)) diag_error(node->source_location, "symbol `%s` already exists", node->tlc_module.name);
            llir_symbol_t *symbol = llir_namespace_add_symbol_module(current_namespace, node->tlc_module.name);
            HLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, pass_one(context, symbol->module.namespace, node));
            break;
        case HLIR_NODE_TYPE_TLC_FUNCTION: break;
        case HLIR_NODE_TYPE_TLC_EXTERN: break;
        case HLIR_NODE_TYPE_TLC_TYPE_DEFINITION:
            if(llir_namespace_exists_type(current_namespace, node->tlc_type_definition.name)) diag_error(node->source_location, "type `%s` already exists", node->tlc_type_definition.name);
            llir_namespace_add_type(current_namespace, node->tlc_type_definition.name);
            break;
        case HLIR_NODE_TYPE_TLC_DECLARATION: break;

        HLIR_CASE_STMT()
        HLIR_CASE_EXPRESSION()
    }
}

// static llir_type_t *evaluate_expr_types(context_t *context, llir_namespace_t *current_namespace, llir_node_t *node) {
//     switch(node->type) {
//         case LLIR_NODE_TYPE_EXPR_LITERAL_NUMERIC: {
//         }
//         case LLIR_NODE_TYPE_EXPR_LITERAL_STRING: {
//         }
//         case LLIR_NODE_TYPE_EXPR_LITERAL_CHAR: {
//         }
//         case LLIR_NODE_TYPE_EXPR_LITERAL_BOOL: {
//         }
//         case LLIR_NODE_TYPE_EXPR_BINARY: {
//         }
//         case LLIR_NODE_TYPE_EXPR_UNARY: {
//         }
//         case LLIR_NODE_TYPE_EXPR_VARIABLE: {
//         }
//         case LLIR_NODE_TYPE_EXPR_TUPLE: {
//         }
//         case LLIR_NODE_TYPE_EXPR_CALL: {
//         }
//         case LLIR_NODE_TYPE_EXPR_CAST: {
//         }
//         case LLIR_NODE_TYPE_EXPR_SUBSCRIPT: {
//         }
//         case LLIR_NODE_TYPE_EXPR_SELECTOR: {
//         }

//         LLIR_CASE_TLC(assert(false));
//         LLIR_CASE_STMT(assert(false));
//     }
//     assert(false);
// }

// static void evaluate_types(context_t *context, llir_namespace_t *current_namespace, llir_node_t *node) {
//     switch(node->type) {
//         case LLIR_NODE_TYPE_ROOT: LLIR_NODE_LIST_FOREACH(&node->root.tlcs, evaluate_types(context, current_namespace, node)); break;

//         case LLIR_NODE_TYPE_TLC_MODULE: {
//             llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_module.name, LLIR_SYMBOL_KIND_MODULE);
//             assert(symbol != NULL);

//             LLIR_NODE_LIST_FOREACH(&node->tlc_module.tlcs, evaluate_types(context, symbol->module.namespace, node));
//         } break;
//         case LLIR_NODE_TYPE_TLC_FUNCTION: {
//             llir_symbol_t *symbol = llir_namespace_find_symbol_with_kind(current_namespace, node->tlc_function.name, LLIR_SYMBOL_KIND_FUNCTION);
//             assert(symbol != NULL);

//             if(node->tlc_function.statement != NULL) evaluate_types(context, current_namespace, node->tlc_function.statement);
//         } break;

//         case LLIR_NODE_TYPE_STMT_BLOCK: LLIR_NODE_LIST_FOREACH(&node->stmt_block.statements, evaluate_types(context, current_namespace, node)); break;
//         case LLIR_NODE_TYPE_STMT_DECLARATION: if(node->stmt_declaration.initial != NULL) evaluate_expr_types(context, current_namespace, node->stmt_declaration.initial); break;
//         case LLIR_NODE_TYPE_STMT_EXPRESSION: evaluate_expr_types(context, current_namespace, node->stmt_expression.expression); break;
//         case LLIR_NODE_TYPE_STMT_RETURN: if(node->stmt_return.value != NULL) evaluate_expr_types(context, current_namespace, node->stmt_return.value); break;
//         case LLIR_NODE_TYPE_STMT_IF:
//             evaluate_expr_types(context, current_namespace, node->stmt_if.condition);
//             if(node->stmt_if.body != NULL) evaluate_types(context, current_namespace, node->stmt_if.body);
//             if(node->stmt_if.else_body != NULL) evaluate_types(context, current_namespace, node->stmt_if.else_body);
//             break;
//         case LLIR_NODE_TYPE_STMT_WHILE:
//             if(node->stmt_while.condition != NULL) evaluate_expr_types(context, current_namespace, node->stmt_while.condition);
//             if(node->stmt_while.body != NULL) evaluate_types(context, current_namespace, node->stmt_while.body);
//             break;

//         LLIR_CASE_EXPRESSION(assert(false));
//     }
// }

// static void print_namespace(llir_namespace_t *namespace, size_t depth) {
//     printf("%*sthis: %#lx\n", depth, "", namespace);
//     printf("%*sparent: %#lx\n", depth, "", namespace->parent);
//     printf("%*stypes:\n", depth, "");
//     for(size_t i = 0; i < namespace->type_count; i++) {
//         printf("%*s  - %s\n", depth, "", namespace->types[i]->name);
//     }
//     printf("%*ssymbols:\n", depth, "");
//     for(size_t i = 0; i < namespace->symbol_count; i++) {
//         printf("%*s  - %s (%#lx)\n", depth, "", namespace->symbols[i]->name, &namespace->symbols[i]);
//         if(namespace->symbols[i]->kind == LLIR_SYMBOL_KIND_MODULE) print_namespace(namespace->symbols[i]->module.namespace, depth + 4);
//     }
// }

llir_node_t *semantics(hlir_node_t *root_node, llir_namespace_t *root_namespace, llir_type_cache_t *anon_type_cache) {
    assert(root_node->type == HLIR_NODE_TYPE_ROOT);

    context_t context = {
        .root_namespace = root_namespace,
        .anon_type_cache = anon_type_cache
    };

    pass_one(&context, context.root_namespace, root_node);
    pass_two(&context, context.root_namespace, root_node);
    llir_node_t *llir_root_node = lower_node(&context, context.root_namespace, root_node);

    return llir_root_node;
}