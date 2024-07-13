#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lexer/token.h"
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
    if(type == NULL) {
        printf("null");
        return;
    }
    switch(type->kind) {
        case IR_TYPE_KIND_INTEGER:
            if(!type->integer.is_signed) printf("u");
            printf("int%lu", type->integer.bit_size);
            break;
        case IR_TYPE_KIND_POINTER:
            printf("*");
            print_type(type->pointer.referred);
            break;
    }
}

static void print_node(ir_node_t *node, int depth) {
    if(node == NULL) return;

    static const char *binary_op_translations[] = {
        "+", "-", "*", "/", "%", ">", ">=", "<", "<=", "==", "!=", "="
    };
    static const char *unary_op_translations[] = {
        "!", "-", "*", "&"
    };

    printf("%*s(", depth * 2, "");
    switch(node->type) {
        case IR_NODE_TYPE_ROOT: printf("root"); break;

        case IR_NODE_TYPE_TLC_FUNCTION: printf("tlc.function `%s`", node->tlc_function.name); break;

        case IR_NODE_TYPE_STMT_BLOCK: printf("stmt.block"); break;
        case IR_NODE_TYPE_STMT_DECLARATION: printf("stmt.declaration `%s` `", node->stmt_declaration.name); print_type(node->stmt_declaration.type); printf("`"); break;
        case IR_NODE_TYPE_STMT_EXPRESSION: printf("stmt.expression"); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: printf("expr.literal_numeric `%lu`", node->expr_literal.numeric_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: printf("expr.literal_string `%s`", node->expr_literal.string_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: printf("expr.literal_char `%c`", node->expr_literal.char_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: printf("expr.literal_bool `%s`", node->expr_literal.bool_value ? "true" : "false"); break;
        case IR_NODE_TYPE_EXPR_BINARY: printf("expr.binary `%s`", binary_op_translations[node->expr_binary.operation]); break;
        case IR_NODE_TYPE_EXPR_UNARY: printf("expr.unary `%s`", unary_op_translations[node->expr_unary.operation]); break;
        case IR_NODE_TYPE_EXPR_VARIABLE: printf("expr.variable `%s`", node->expr_variable.name); break;
        case IR_NODE_TYPE_EXPR_CALL: printf("expr.call `%s`", node->expr_call.function_name); break;
    }
    printf(")\n");

    depth++;
    switch(node->type) {
        case IR_NODE_TYPE_ROOT: print_list(&node->root.tlc_nodes, depth); break;

        case IR_NODE_TYPE_TLC_FUNCTION: print_list(&node->tlc_function.statements, depth); break;

        case IR_NODE_TYPE_STMT_BLOCK: print_list(&node->stmt_block.statements, depth); break;
        case IR_NODE_TYPE_STMT_DECLARATION: print_node(node->stmt_declaration.initial, depth); break;
        case IR_NODE_TYPE_STMT_EXPRESSION: print_node(node->stmt_expression.expression, depth); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: break;
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: break;
        case IR_NODE_TYPE_EXPR_BINARY: print_node(node->expr_binary.left, depth); print_node(node->expr_binary.right, depth); break;
        case IR_NODE_TYPE_EXPR_UNARY: print_node(node->expr_unary.operand, depth); break;
        case IR_NODE_TYPE_EXPR_VARIABLE: break;
        case IR_NODE_TYPE_EXPR_CALL: print_list(&node->expr_call.arguments, depth); break;
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
    source_free(source);

    print_node(node, 0);
    return EXIT_SUCCESS;
}