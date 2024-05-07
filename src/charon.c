#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/stat.h>
#include "ir/node.h"
#include "lexer/token.h"
#include "lexer/tokenizer.h"
#include "parser/parser.h"

[[noreturn]] void exit_perror() {
    perror("ERROR(main): ");
    exit(EXIT_FAILURE);
    __builtin_unreachable();
}

[[noreturn]] void exit_message(char *msg) {
    fprintf(stderr, "ERROR(main): %s\n", msg);
    exit(EXIT_FAILURE);
    __builtin_unreachable();
}

static void print_node([[maybe_unused]] void *ctx, ir_node_t *node, size_t depth) {
    static const char *binary_op_translations[] = {
        "+", "-", "*", "/", "%", ">", ">=", "<", "<=", "==", "!=", "="
    };
    static const char *unary_op_translations[] = {
        "!", "-"
    };

    printf("%*s", depth * 2, "");
    switch(node->type) {
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

    ir_node_recurse(NULL, ast, print_node);

    tokenizer_free(tokenizer);
    free(source);

    return EXIT_SUCCESS;
}