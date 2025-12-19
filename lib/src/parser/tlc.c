#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "parse.h"
#include "parser/parser.h"

#include <string.h>

static void parse_import(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_IMPORT);
    parser_consume(parser, CHARON_TOKEN_KIND_LITERAL_STRING);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);

    parser_close_element(parser, CHARON_NODE_KIND_TLC_IMPORT);
}

static void parse_attribute(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_AT);
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT)) {
        if(parser_peek(parser) != CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT) {
            parse_expr(parser);
            while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA)) {
                parse_expr(parser);
            }
        }
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
    }

    parser_close_element(parser, CHARON_NODE_KIND_STMT_ATTRIBUTE);
}

static void parse_type_definition(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_TYPE);
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_CARET_LEFT)) {
        do {
            parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
        } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT);
    }
    parse_type(parser);

    parser_close_element(parser, CHARON_NODE_KIND_TLC_TYPE_DEFINITION);
}

static void parse_module(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_MODULE);
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
    while(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
        if(parser_is_eof(parser)) {
            parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT);
            break;
        }
        parse_tlc(parser);
    }

    parser_close_element(parser, CHARON_NODE_KIND_TLC_MODULE);
}

static void parse_function(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_FUNCTION);
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_CARET_LEFT)) {
        do {
            parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
        } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT);
    }
    parse_type_function(parser);
    parse_stmt(parser);

    parser_close_element(parser, CHARON_NODE_KIND_TLC_FUNCTION);
}

static void parse_extern(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_EXTERN);
    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_FUNCTION);
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    parse_type_function(parser);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);

    parser_close_element(parser, CHARON_NODE_KIND_TLC_EXTERN);
}

static void parse_declaration(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_LET);
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_COLON);
    parse_type(parser);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_EQUAL)) parse_expr(parser);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);

    parser_close_element(parser, CHARON_NODE_KIND_TLC_DECLARATION);
}

static void parse_enum(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_ENUM);
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COLON)) parse_type(parser);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
    if(parser_peek(parser) == CHARON_TOKEN_KIND_IDENTIFIER) {
        do {
            parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
            if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_EQUAL)) parse_expr(parser);
        } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
    }
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT);

    parser_close_element(parser, CHARON_NODE_KIND_TLC_ENUMERATION);
}

void parse_tlc(charon_parser_t *parser) {
    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_KEYWORD_IMPORT:   parse_import(parser); break;
        case CHARON_TOKEN_KIND_PNCT_AT:          parse_attribute(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_MODULE:   parse_module(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_FUNCTION: parse_function(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_EXTERN:   parse_extern(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_TYPE:     parse_type_definition(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_LET:      parse_declaration(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_ENUM:     parse_enum(parser); break;
        default:                                 {
            charon_token_kind_t kinds[7] = { CHARON_TOKEN_KIND_PNCT_AT,      CHARON_TOKEN_KIND_KEYWORD_MODULE, CHARON_TOKEN_KIND_KEYWORD_FUNCTION, CHARON_TOKEN_KIND_KEYWORD_EXTERN,
                                             CHARON_TOKEN_KIND_KEYWORD_TYPE, CHARON_TOKEN_KIND_KEYWORD_LET,    CHARON_TOKEN_KIND_KEYWORD_ENUM };

            charon_diag_data_t *diag_data = malloc(sizeof(charon_diag_data_t) + sizeof(kinds));
            diag_data->unexpected_token.found = parser_peek(parser);
            diag_data->unexpected_token.expected_count = sizeof(kinds) / sizeof(kinds[0]);
            memcpy(&diag_data->unexpected_token.expected, kinds, sizeof(kinds));

            parser_error(parser, CHARON_DIAG_UNEXPECTED_TOKEN, diag_data);
            break;
        }
    }
}
