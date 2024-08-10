#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "lexer/tokenizer.h"
#include "parser/parser.h"

static void print_node(ir_node_t *node, int depth);

static void print_list(ir_node_list_t *list, int depth) {
    ir_node_t *node = list->first;
    while(node != NULL) {
        print_node(node, depth);
        node = node->next;
    }
}

static void print_type(ir_type_t *type) {
    assert(type != NULL);
    switch(type->kind) {
        case IR_TYPE_KIND_VOID: printf("void"); break;
        case IR_TYPE_KIND_INTEGER:
            if(!type->integer.is_signed) printf("u");
            printf("int%lu", type->integer.bit_size);
            break;
        case IR_TYPE_KIND_POINTER:
            printf("*");
            print_type(type->pointer.referred);
            break;
        case IR_TYPE_KIND_TUPLE:
            printf("(");
            for(size_t i = 0; i < type->tuple.type_count; i++) {
                print_type(type->tuple.types[i]);
                if(i < type->tuple.type_count - 1) printf(", ");
            }
            printf(")");
            break;
    }
}

static void print_node(ir_node_t *node, int depth) {
    if(node == NULL) return;

    static const char *binary_op_translations[] = {
        "=", "+", "-", "*", "/", "%", ">", ">=", "<", "<=", "==", "!="
    };
    static const char *unary_op_translations[] = {
        "!", "-", "*", "&"
    };

    printf("%*s(", depth * 2, "");
    switch(node->type) {
        case IR_NODE_TYPE_ROOT: printf("root"); break;

        case IR_NODE_TYPE_TLC_MODULE: printf("tlc.module `%s`", node->tlc_module.name); break;
        case IR_NODE_TYPE_TLC_FUNCTION: printf("tlc.function `%s` `", node->tlc_function.prototype->name); print_type(node->tlc_function.prototype->return_type); printf("`"); if(node->tlc_function.prototype->varargs) printf(" varargs"); break;
        case IR_NODE_TYPE_TLC_EXTERN: printf("tlc.extern `%s` `", node->tlc_extern.prototype->name); print_type(node->tlc_extern.prototype->return_type); printf("`"); if(node->tlc_extern.prototype->varargs) printf(" varargs"); break;

        case IR_NODE_TYPE_STMT_NOOP: printf("stmt.noop"); break;
        case IR_NODE_TYPE_STMT_BLOCK: printf("stmt.block"); break;
        case IR_NODE_TYPE_STMT_DECLARATION:
            printf("stmt.declaration `%s`", node->stmt_declaration.name);
            if(node->stmt_declaration.type != NULL) {
                printf(" `");
                print_type(node->stmt_declaration.type);
                printf("`");
            }
            break;
        case IR_NODE_TYPE_STMT_EXPRESSION: printf("stmt.expression"); break;
        case IR_NODE_TYPE_STMT_RETURN: printf("stmt.return"); break;
        case IR_NODE_TYPE_STMT_IF: printf("stmt.if"); break;
        case IR_NODE_TYPE_STMT_WHILE: printf("stmt.while"); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: printf("expr.literal_numeric `%lu`", node->expr_literal.numeric_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: printf("expr.literal_string `%s`", node->expr_literal.string_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: printf("expr.literal_char `%c`", node->expr_literal.char_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: printf("expr.literal_bool `%s`", node->expr_literal.bool_value ? "true" : "false"); break;
        case IR_NODE_TYPE_EXPR_BINARY: printf("expr.binary `%s`", binary_op_translations[node->expr_binary.operation]); break;
        case IR_NODE_TYPE_EXPR_UNARY: printf("expr.unary `%s`", unary_op_translations[node->expr_unary.operation]); break;
        case IR_NODE_TYPE_EXPR_VARIABLE: printf("expr.variable `%s`", node->expr_variable.name); break;
        case IR_NODE_TYPE_EXPR_CALL: printf("expr.call `%s`", node->expr_call.function_name); break;
        case IR_NODE_TYPE_EXPR_TUPLE: printf("expr.tuple"); break;
        case IR_NODE_TYPE_EXPR_CAST: printf("expr.cast `"); print_type(node->expr_cast.type); printf("`"); break;
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX: printf("expr.access_index"); break;
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX_CONST: printf("expr.access_index_const `%lu`", node->expr_access_index_const.index); break;
        case IR_NODE_TYPE_EXPR_SELECTOR: printf("expr.selector `%s`", node->expr_selector.name); break;
    }
    printf(")\n");

    depth++;
    switch(node->type) {
        case IR_NODE_TYPE_ROOT: print_list(&node->root.tlc_nodes, depth); break;

        case IR_NODE_TYPE_TLC_MODULE: print_list(&node->tlc_module.tlcs, depth); break;
        case IR_NODE_TYPE_TLC_FUNCTION:
            if(node->tlc_function.prototype->argument_count > 0) {
                printf("%*s", depth * 2, "");
                printf("args:\n");
                for(size_t i = 0; i < node->tlc_function.prototype->argument_count; i++) {
                    ir_function_argument_t *argument = &node->tlc_function.prototype->arguments[i];
                    printf("%*s", (depth + 1) * 2, "");
                    printf("- %s `", argument->name);
                    print_type(argument->type);
                    printf("`\n");
                }
            }
            if(node->tlc_function.statements.count > 0) {
                printf("%*s", depth * 2, "");
                printf("body:\n");
                print_list(&node->tlc_function.statements, depth + 1);
            }
            break;
        case IR_NODE_TYPE_TLC_EXTERN:
            if(node->tlc_function.prototype->argument_count > 0) {
                printf("%*s", depth * 2, "");
                printf("args:\n");
                for(size_t i = 0; i < node->tlc_function.prototype->argument_count; i++) {
                    ir_function_argument_t *argument = &node->tlc_function.prototype->arguments[i];
                    printf("%*s", (depth + 1) * 2, "");
                    printf("- %s `", argument->name);
                    print_type(argument->type);
                    printf("`\n");
                }
            }
            break;

        case IR_NODE_TYPE_STMT_NOOP: break;
        case IR_NODE_TYPE_STMT_BLOCK: print_list(&node->stmt_block.statements, depth); break;
        case IR_NODE_TYPE_STMT_DECLARATION: print_node(node->stmt_declaration.initial, depth); break;
        case IR_NODE_TYPE_STMT_EXPRESSION: print_node(node->stmt_expression.expression, depth); break;
        case IR_NODE_TYPE_STMT_RETURN: if(node->stmt_return.value != NULL) print_node(node->stmt_return.value, depth); break;
        case IR_NODE_TYPE_STMT_IF:
            print_node(node->stmt_if.condition, depth);
            print_node(node->stmt_if.body, depth);
            if(node->stmt_if.else_body != NULL) print_node(node->stmt_if.else_body, depth);
            break;
        case IR_NODE_TYPE_STMT_WHILE:
            print_node(node->stmt_while.condition, depth);
            print_node(node->stmt_while.body, depth);
            break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: break;
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: break;
        case IR_NODE_TYPE_EXPR_BINARY: print_node(node->expr_binary.left, depth); print_node(node->expr_binary.right, depth); break;
        case IR_NODE_TYPE_EXPR_UNARY: print_node(node->expr_unary.operand, depth); break;
        case IR_NODE_TYPE_EXPR_VARIABLE: break;
        case IR_NODE_TYPE_EXPR_CALL: print_list(&node->expr_call.arguments, depth); break;
        case IR_NODE_TYPE_EXPR_TUPLE: print_list(&node->expr_tuple.values, depth); break;
        case IR_NODE_TYPE_EXPR_CAST: print_node(node->expr_cast.value, depth); break;
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX:
            print_node(node->expr_access_index.value, depth);
            print_node(node->expr_access_index.index, depth);
            break;
        case IR_NODE_TYPE_EXPR_ACCESS_INDEX_CONST: print_node(node->expr_access_index_const.value, depth); break;
        case IR_NODE_TYPE_EXPR_SELECTOR: print_node(node->expr_selector.value, depth); break;
    }
}

int main(int argc, char **argv) {
    if(argc < 3) {
        printf("missing arguments\n");
        return EXIT_FAILURE;
    }

    source_t *source = source_from_path(argv[1]);
    tokenizer_t *tokenizer = tokenizer_make(source);

    ir_node_t *node;
    if(strcmp(argv[2], "root") == 0) node = parser_root(tokenizer);
    else if(strcmp(argv[2], "tlc") == 0) node = parser_tlc(tokenizer);
    else if(strcmp(argv[2], "stmt") == 0) node = parser_stmt(tokenizer);
    else if(strcmp(argv[2], "expr") == 0) node = parser_expr(tokenizer);
    else {
        printf("invalid parser `%s`\n", argv[2]);
        return EXIT_FAILURE;
    }

    tokenizer_free(tokenizer);

    print_node(node, 0);

    source_free(source);
    return EXIT_SUCCESS;
}