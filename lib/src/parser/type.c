#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "parser/parse.h"
#include "parser/parser.h"

void parse_type(charon_parser_t *parser) {
    parser_open_element(parser);

    // Struct type
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_KEYWORD_STRUCT)) {
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
        if(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
            do {
                parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
                parser_consume(parser, CHARON_TOKEN_KIND_PNCT_COLON);
                parse_type(parser);
            } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
            parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT);
        }
        return parser_close_element(parser, CHARON_NODE_KIND_TYPE_STRUCT);
    }

    // Tuple type
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT)) {
        do {
            parse_type(parser);
        } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
        return parser_close_element(parser, CHARON_NODE_KIND_TYPE_TUPLE);
    }

    // Array type
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACKET_LEFT)) {
        if(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACKET_RIGHT)) {
            parse_expr(parser);
            parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACKET_RIGHT);
        }
        parse_type(parser);
        return parser_close_element(parser, CHARON_NODE_KIND_TYPE_ARRAY);
    }

    // Pointer type
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_STAR)) {
        parse_type(parser);
        return parser_close_element(parser, CHARON_NODE_KIND_TYPE_POINTER);
    }

    // Function reference type
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_KEYWORD_FUNCTION)) {
        parse_type_function(parser);
        return parser_close_element(parser, CHARON_NODE_KIND_TYPE_FUNCTION_REF);
    }

    // Type reference
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_DOUBLE_COLON)) {
        parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    }

    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_CARET_LEFT)) {
        do {
            parse_type(parser);
        } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT);
    }

    parser_close_element(parser, CHARON_NODE_KIND_TYPE_REFERENCE);
}

void parse_type_function(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);
    if(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT)) {
        do {
            if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_TRIPLE_PERIOD)) break;
            parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
            parser_consume(parser, CHARON_TOKEN_KIND_PNCT_COLON);
            parse_type(parser);
        } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
    }
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COLON)) parse_type(parser);

    parser_close_element(parser, CHARON_NODE_KIND_TYPE_FUNCTION);
}
