#include "core/db.h"
#include "core/diag.h"
#include "query/input.h"
#include "query/queries.h"
#include "query/query.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static query_input_t input_for(const file_t *file) {
    return (query_input_t) { .kind = QUERY_INPUT_FILE, .ptr = file };
}

static void print_text(const text_t *text) {
    if(text == nullptr) {
        printf("<null>");
        return;
    }
    printf("%.*s", (int) text->size, (const char *) text->data);
}

static void print_element(const element_inner_t *element) {
    switch(element->type) {
        case ELEMENT_TYPE_NODE:
            for(size_t i = 0; i < element->node.child_count; i++) print_element(element->node.children[i]);
            break;
        case ELEMENT_TYPE_TOKEN:
            for(size_t i = 0; i < element->token.leading_trivia_count; i++) print_element(element->token.trivia[i]);

            switch(element->token.kind) {
                case CHARON_TOKEN_KIND_KEYWORD_RETURN:
                case CHARON_TOKEN_KIND_KEYWORD_IF:
                case CHARON_TOKEN_KIND_KEYWORD_ELSE:
                case CHARON_TOKEN_KIND_KEYWORD_WHILE:
                case CHARON_TOKEN_KIND_KEYWORD_FUNCTION:
                case CHARON_TOKEN_KIND_KEYWORD_LET:
                case CHARON_TOKEN_KIND_KEYWORD_AS:
                case CHARON_TOKEN_KIND_KEYWORD_EXTERN:
                case CHARON_TOKEN_KIND_KEYWORD_MODULE:
                case CHARON_TOKEN_KIND_KEYWORD_TYPE:
                case CHARON_TOKEN_KIND_KEYWORD_STRUCT:
                case CHARON_TOKEN_KIND_KEYWORD_CONTINUE:
                case CHARON_TOKEN_KIND_KEYWORD_BREAK:
                case CHARON_TOKEN_KIND_KEYWORD_ENUM:
                case CHARON_TOKEN_KIND_KEYWORD_FOR:
                case CHARON_TOKEN_KIND_KEYWORD_SIZEOF:
                case CHARON_TOKEN_KIND_KEYWORD_SWITCH:
                case CHARON_TOKEN_KIND_KEYWORD_DEFAULT:
                case CHARON_TOKEN_KIND_KEYWORD_IMPORT:         printf("\x1b[34m"); break;
                case CHARON_TOKEN_KIND_LITERAL_NUMBER_HEX:
                case CHARON_TOKEN_KIND_LITERAL_NUMBER_BIN:
                case CHARON_TOKEN_KIND_LITERAL_NUMBER_OCT:
                case CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC:     printf("\x1b[92m"); break;
                case CHARON_TOKEN_KIND_LITERAL_STRING:
                case CHARON_TOKEN_KIND_LITERAL_STRING_RAW:
                case CHARON_TOKEN_KIND_LITERAL_CHAR:           printf("\x1b[31m"); break;
                case CHARON_TOKEN_KIND_LITERAL_BOOL:           break;
                case CHARON_TOKEN_KIND_IDENTIFIER:             printf("\x1b[35m"); break;
                case CHARON_TOKEN_KIND_PNCT_TRIPLE_PERIOD:
                case CHARON_TOKEN_KIND_PNCT_DOUBLE_PERIOD:
                case CHARON_TOKEN_KIND_PNCT_PERIOD:
                case CHARON_TOKEN_KIND_PNCT_DOUBLE_COLON:
                case CHARON_TOKEN_KIND_PNCT_COLON:
                case CHARON_TOKEN_KIND_PNCT_SEMI_COLON:
                case CHARON_TOKEN_KIND_PNCT_ARROW:
                case CHARON_TOKEN_KIND_PNCT_THICK_ARROW:
                case CHARON_TOKEN_KIND_PNCT_DOUBLE_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_PLUS_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_MINUS_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_SLASH_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_STAR_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_PERCENTAGE_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_NOT_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_NOT:
                case CHARON_TOKEN_KIND_PNCT_SLASH:
                case CHARON_TOKEN_KIND_PNCT_PERCENTAGE:
                case CHARON_TOKEN_KIND_PNCT_STAR:
                case CHARON_TOKEN_KIND_PNCT_PLUS:
                case CHARON_TOKEN_KIND_PNCT_MINUS:
                case CHARON_TOKEN_KIND_PNCT_GREATER_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_SHIFT_RIGHT:
                case CHARON_TOKEN_KIND_PNCT_CARET_RIGHT:
                case CHARON_TOKEN_KIND_PNCT_LESS_EQUAL:
                case CHARON_TOKEN_KIND_PNCT_SHIFT_LEFT:
                case CHARON_TOKEN_KIND_PNCT_CARET_LEFT:
                case CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT:
                case CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT:
                case CHARON_TOKEN_KIND_PNCT_BRACE_LEFT:
                case CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT:
                case CHARON_TOKEN_KIND_PNCT_BRACKET_LEFT:
                case CHARON_TOKEN_KIND_PNCT_BRACKET_RIGHT:
                case CHARON_TOKEN_KIND_PNCT_COMMA:
                case CHARON_TOKEN_KIND_PNCT_LOGICAL_AND:
                case CHARON_TOKEN_KIND_PNCT_LOGICAL_OR:
                case CHARON_TOKEN_KIND_PNCT_AMPERSAND:
                case CHARON_TOKEN_KIND_PNCT_PIPE:
                case CHARON_TOKEN_KIND_PNCT_CARET:
                case CHARON_TOKEN_KIND_PNCT_AT:                printf("\x1b[90m"); break;
                default:                                       break;
            }

            print_text(element->token.text);

            printf("\x1b[0m");

            for(size_t i = 0; i < element->token.trailing_trivia_count; i++) print_element(element->token.trivia[element->token.leading_trivia_count + i]);
            break;
        case ELEMENT_TYPE_TRIVIA: print_text(element->trivia.text); break;
    }
}

static void print_tree(const element_inner_t *element, int depth) {
    for(int i = 0; i < depth * 4; i++) printf(" ");

    switch(element->type) {
        case ELEMENT_TYPE_TRIVIA: assert(false);
        case ELEMENT_TYPE_NODE:   {
            charon_node_kind_t node_kind = element->node.kind;

            printf("%s%s%s\n", node_kind == CHARON_NODE_KIND_ERROR ? "\e[41m" : "", charon_node_kind_tostring(node_kind), "\e[0m");

            size_t child_count = element->node.child_count;
            for(size_t i = 0; i < child_count; i++) { print_tree(element->node.children[i], depth + 1); }
            break;
        }
        case ELEMENT_TYPE_TOKEN:
            charon_token_kind_t token_kind = element->token.kind;
            const char *kind_text = charon_token_kind_tostring(token_kind);

            printf("Token(%s", kind_text);

            if(element->token.text != nullptr) {
                printf(", `");
                print_text(element->token.text);
                printf("`");
            }

            printf(")\n");
            break;
    }
}

static void print_tokens(const element_array_t *tokens) {
    printf("  FILE_TOKENS: %zu\n", tokens->count);
    for(size_t i = 0; i < tokens->count; i++) {
        const element_inner_t *token = tokens->tokens[i];
        printf("    [%zu] %s `", i, charon_token_kind_tostring(token->token.kind));
        print_text(token->token.text);
        printf("`\n");
    }
}

static void print_items(const decl_array_t *items) {
    printf("  FILE_ITEMS: %zu\n", items->count);
    for(size_t i = 0; i < items->count; i++) {
        const element_inner_t *item = items->items[i];
        printf("    [%zu] %s\n", i, item->type == ELEMENT_TYPE_NODE ? charon_node_kind_tostring(item->node.kind) : "?");
    }
}

static void print_diagnostics(db_t *db, const diag_array_t *diagnostics) {
    printf("  FILE_DIAGNOSTICS: %zu\n", diagnostics->count);
    for(size_t i = 0; i < diagnostics->count; i++) {
        const diag_item_t *item = diagnostics->items[i];
        const file_t *file = db_file_from_handle(db, item->path->file);

        printf("    [%zu] kind=%d path=", i, (int) item->kind);
        if(file == nullptr)
            printf("<unknown>");
        else
            print_text(file->path);

        printf(" indices=[");
        for(size_t j = 0; j < item->path->length; j++) printf("%s%zu", j == 0 ? "" : ", ", item->path->indices[j]);
        printf("]\n");
    }
}

void charon_test(const char *path) {
    db_t *db = db_new();

    const file_t *file = db_add_file(db, path);
    if(file == nullptr) {
        db_destroy(db);
        return;
    }

    query_input_t input = input_for(file);

    printf("== colored ==\n");
    query_parse_result_t *parse = (query_parse_result_t *) query_execute(db, QUERY_FILE_PARSE, &input);
    print_element(parse->root);
    printf("\n");

    printf("== tokens ==\n");
    element_array_t *tokens = (element_array_t *) query_execute(db, QUERY_FILE_TOKENS, &input);
    print_tokens(tokens);

    printf("== items ==\n");
    decl_array_t *items = (decl_array_t *) query_execute(db, QUERY_FILE_ITEMS, &input);
    print_items(items);

    printf("== tree ==\n");
    print_tree(parse->root, 0);

    printf("== diagnostics ==\n");
    diag_array_t *diagnostics = (diag_array_t *) query_execute(db, QUERY_FILE_DIAGNOSTICS, &input);
    print_diagnostics(db, diagnostics);

    db_destroy(db);
}
