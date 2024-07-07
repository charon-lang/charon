#include "lib/source.h"
#include "lexer/tokenizer.h"
#include "parser/parser.h"

#include <stdio.h>
#include <stdlib.h>

static void print_node(ir_node_t *node, int depth);

int main(int argc, char **argv) {
    printf("Charon (dev)\n");

    source_t *source = source_from_path("tests/00.charon");
    tokenizer_t *tokenizer = tokenizer_make(source);

    ir_node_t *root = parser_root(tokenizer);
    print_node(root, 0);

    tokenizer_free(tokenizer);
    source_free(source);

    return EXIT_SUCCESS;
}

static void print_list(ir_node_list_t *list, int depth) {
    ir_node_t *node = list->first;
    while(node != NULL) {
        print_node(node, depth);
        node = node->next;
    }
}

static void print_node(ir_node_t *node, int depth) {
    static const char *binary_op_translations[] = {
        "+", "-", "*", "/", "%", ">", ">=", "<", "<=", "==", "!=", "="
    };
    static const char *unary_op_translations[] = {
        "!", "-", "*", "&"
    };

    printf("%*s", depth * 2, "");
    switch(node->type) {
        case IR_NODE_TYPE_ROOT: printf("(root)"); break;

        case IR_NODE_TYPE_TLC_FUNCTION: printf("(fn %s)", node->tlc_function.name); break;

        case IR_NODE_TYPE_STMT_EXPRESSION: printf("(stmt_expr)"); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: printf("(literal_numeric `%lu`)", node->expr_literal.numeric_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: printf("(literal_string `%s`)", node->expr_literal.string_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: printf("(literal_char `%c`)", node->expr_literal.char_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: printf("(literal_bool `%s`)", node->expr_literal.bool_value ? "true" : "false"); break;
        case IR_NODE_TYPE_EXPR_BINARY: printf("(binary `%s`)", binary_op_translations[node->expr_binary.operation]); break;
        case IR_NODE_TYPE_EXPR_UNARY: printf("(unary `%s`)", unary_op_translations[node->expr_unary.operation]); break;
    }
    printf("\n");

    depth++;
    switch(node->type) {
        case IR_NODE_TYPE_ROOT: print_list(&node->root.tlc_nodes, depth); break;

        case IR_NODE_TYPE_TLC_FUNCTION: print_list(&node->tlc_function.statements, depth); break;

        case IR_NODE_TYPE_STMT_EXPRESSION: print_node(node->stmt_expression.expression, depth); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: break;
        case IR_NODE_TYPE_EXPR_LITERAL_BOOL: break;
        case IR_NODE_TYPE_EXPR_BINARY: print_node(node->expr_binary.left, depth); print_node(node->expr_binary.right, depth); break;
        case IR_NODE_TYPE_EXPR_UNARY: print_node(node->expr_unary.operand, depth); break;
    }
}