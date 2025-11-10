#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "parse.h"
#include "parser/parser.h"

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
    while(!parser_is_eof(parser) && !parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
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

    parser_close_element(parser, CHARON_NODE_KIND_STMT_DECLARATION);
}

static void parse_enum(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_ENUM);
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
    if(parser_peek(parser) == CHARON_TOKEN_KIND_IDENTIFIER) {
        do {
            parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
        } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
    }
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT);

    parser_close_element(parser, CHARON_NODE_KIND_TLC_ENUMERATION);
}

void parse_tlc(charon_parser_t *parser) {
    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_KEYWORD_MODULE:   return parse_module(parser);
        case CHARON_TOKEN_KIND_KEYWORD_FUNCTION: return parse_function(parser);
        case CHARON_TOKEN_KIND_KEYWORD_EXTERN:   return parse_extern(parser);
        case CHARON_TOKEN_KIND_KEYWORD_TYPE:     return parse_type_definition(parser);
        case CHARON_TOKEN_KIND_KEYWORD_LET:      return parse_declaration(parser);
        case CHARON_TOKEN_KIND_KEYWORD_ENUM:     return parse_enum(parser);
        default:                                 return parser_unexpected_error(parser);
    }
}
