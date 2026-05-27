#include "charon/lexer.h"
#include "common/text.h"
#include "core/db.h"
#include "core/store.h"
#include "lexer_pipeline.h"
#include "parser/parser.h"
#include "syntax/element.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

void print_text(const text_t *text) {
    printf("%.*s", (int) text->size, text->data);
}

void print_element(db_t *db, const element_inner_t *element) {
    switch(element->type) {
        case ELEMENT_TYPE_NODE:
            for(size_t i = 0; i < element->node.child_count; i++) print_element(db, store_get(db->store, element->node.children[i]));
            break;
        case ELEMENT_TYPE_TOKEN:
            for(size_t i = 0; i < element->token.leading_trivia_count; i++) print_element(db, store_get(db->store, element->token.trivia[i]));

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

            print_text(store_get(db->store, element->token.text));

            printf("\x1b[0m");

            for(size_t i = 0; i < element->token.trailing_trivia_count; i++) print_element(db, store_get(db->store, element->token.trivia[element->token.leading_trivia_count + i]));
            break;
        case ELEMENT_TYPE_TRIVIA: print_text(store_get(db->store, element->trivia.text)); break;
    }
}

static void print_tree(db_t *db, const element_inner_t *element, int depth) {
    for(int i = 0; i < depth * 4; i++) printf(" ");

    switch(element->type) {
        case ELEMENT_TYPE_TRIVIA: assert(false);
        case ELEMENT_TYPE_NODE:   {
            charon_node_kind_t node_kind = element->node.kind;

            printf("%s%s%s\n", node_kind == CHARON_NODE_KIND_ERROR ? "\e[41m" : "", charon_node_kind_tostring(node_kind), "\e[0m");

            size_t child_count = element->node.child_count;
            for(size_t i = 0; i < child_count; i++) { print_tree(db, store_get(db->store, element->node.children[i]), depth + 1); }
            break;
        }
        case ELEMENT_TYPE_TOKEN:
            charon_token_kind_t token_kind = element->token.kind;
            const char *kind_text = charon_token_kind_tostring(token_kind);

            printf("Token(%s", kind_text);
            printf(", `");
            print_text(store_get(db->store, element->token.text));
            printf("`");
            printf(")\n");
            break;
    }
}

void charon_test(const void *data, size_t data_size) {
    db_t *db = db_new();

    charon_lexer_t *lexer = charon_lexer_new(data, data_size);
    lexer_pipeline_t *pipeline = lexer_pipeline_make(db, lexer);

    parser_t *parser = parser_make(db, pipeline);

    parser_output_t output = parser_parse_root(parser);

    print_tree(db, store_get(db->store, output.root_element), 0);
    print_element(db, store_get(db->store, output.root_element));

    parser_destroy(parser);

    lexer_pipeline_destroy(pipeline);
    charon_lexer_destroy(lexer);
}
