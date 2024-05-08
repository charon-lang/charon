#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/stat.h>
#include "ir/node.h"
#include "lexer/token.h"
#include "lexer/tokenizer.h"
#include "parser/parser.h"
#include "semantics/typecheck.h"

[[noreturn]] void exit_perror() {
    perror("ERROR(main): ");
    exit(EXIT_FAILURE);
}

[[noreturn]] void exit_message(char *msg) {
    fprintf(stderr, "ERROR(main): %s\n", msg);
    exit(EXIT_FAILURE);
}

static void print_node(ir_node_t *node, int depth) {
    static const char *binary_op_translations[] = {
        "+", "-", "*", "/", "%", ">", ">=", "<", "<=", "==", "!=", "="
    };
    static const char *unary_op_translations[] = {
        "!", "-"
    };

    printf("%*s", depth * 2, "");
    switch(node->type) {
        case IR_NODE_TYPE_PROGRAM: printf("(program)"); break;
        case IR_NODE_TYPE_FUNCTION: printf("(function %s)", node->function.name); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: printf("(literal_numeric %lu)", node->expr_literal.numeric_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: printf("(literal_string \"%s\")", node->expr_literal.string_value); break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: printf("(literal_char '%c')", node->expr_literal.char_value); break;
        case IR_NODE_TYPE_EXPR_BINARY: printf("(binary %s)", binary_op_translations[node->expr_binary.operation]); break;
        case IR_NODE_TYPE_EXPR_UNARY: printf("(unary %s)", unary_op_translations[node->expr_unary.operation]); break;
        case IR_NODE_TYPE_EXPR_VAR: printf("(var %s)", node->expr_var.name); break;
        case IR_NODE_TYPE_EXPR_CALL: printf("(call %s)", node->expr_call.name); break;

        case IR_NODE_TYPE_STMT_BLOCK: printf("(block)"); break;
        case IR_NODE_TYPE_STMT_RETURN: printf("(return)"); break;
        case IR_NODE_TYPE_STMT_IF: printf("(if)"); break;
        case IR_NODE_TYPE_STMT_DECL: printf("(decl %s)", node->stmt_decl.name); break;
    }
    printf("\n");

    depth++;
    switch(node->type) {
        case IR_NODE_TYPE_PROGRAM:
            for(size_t i = 0; i < node->program.function_count; i++) print_node(node->program.functions[i], depth);
            break;
        case IR_NODE_TYPE_FUNCTION: print_node(node->function.body, depth); break;

        case IR_NODE_TYPE_EXPR_LITERAL_NUMERIC: break;
        case IR_NODE_TYPE_EXPR_LITERAL_STRING: break;
        case IR_NODE_TYPE_EXPR_LITERAL_CHAR: break;
        case IR_NODE_TYPE_EXPR_BINARY:
            print_node(node->expr_binary.left, depth);
            print_node(node->expr_binary.right, depth);
            break;
        case IR_NODE_TYPE_EXPR_UNARY: print_node(node->expr_unary.operand, depth); break;
        case IR_NODE_TYPE_EXPR_VAR: break;
        case IR_NODE_TYPE_EXPR_CALL:
            for(size_t i = 0; i < node->expr_call.argument_count; i++) print_node(node->expr_call.arguments[i], depth);
            break;

        case IR_NODE_TYPE_STMT_BLOCK:
            for(size_t i = 0; i < node->stmt_block.statement_count; i++) print_node(node->stmt_block.statements[i], depth);
            break;
        case IR_NODE_TYPE_STMT_RETURN: if(node->stmt_return.value != NULL) print_node(node->stmt_return.value, depth); break;
        case IR_NODE_TYPE_STMT_IF:
            print_node(node->stmt_if.condition, depth);
            print_node(node->stmt_if.body, depth);
            if(node->stmt_if.else_body != NULL) print_node(node->stmt_if.else_body, depth);
            break;
        case IR_NODE_TYPE_STMT_DECL: if(node->stmt_decl.initial != NULL) print_node(node->stmt_decl.initial, depth); break;
    }
}

int main(int argc, char **argv) {
    printf("Charon (dev)\n");

    char *source_path = "tests/00.charon";
    char *source_filename = basename(source_path);

    FILE *file = fopen(source_path, "r");
    struct stat s;
    if(file == NULL || fstat(fileno(file), &s) != 0) exit_perror();

    void *data = malloc(s.st_size);
    if(fread(data, 1, s.st_size, file) != s.st_size) exit_perror();
    fclose(file);

    source_t *source = malloc(sizeof(source_t));
    source->name = source_filename;
    source->data = data;
    source->data_length = s.st_size;

    tokenizer_t *tokenizer = tokenizer_make(source);
    if(tokenizer == NULL) exit_message("failed to initialize the tokenizer");
    ir_node_t *ast = parser_parse(tokenizer);
    tokenizer_free(tokenizer);

    print_node(ast, 0);

    typecheck(ast);

    free(source);
    return EXIT_SUCCESS;
}