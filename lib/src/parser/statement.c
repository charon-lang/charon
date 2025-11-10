#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "parse.h"
#include "parser/parser.h"

static void parse_expression(charon_parser_t *parser) {
    parser_syncset_t prev_syncset = parser->syncset;
    parser->syncset.token_kinds[CHARON_TOKEN_KIND_PNCT_SEMI_COLON] = true;

    parser_open_element(parser);

    parse_expr(parser);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);

    parser_close_element(parser, CHARON_NODE_KIND_STMT_EXPRESSION);

    parser->syncset = prev_syncset;
}

static void parse_declaration(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_LET);
    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COLON)) parse_type(parser);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_EQUAL)) parse_expr(parser);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);

    parser_close_element(parser, CHARON_NODE_KIND_STMT_DECLARATION);
}

static void parse_return(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_RETURN);
    if(parser_peek(parser) != CHARON_TOKEN_KIND_PNCT_SEMI_COLON) parse_expr(parser);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);

    parser_close_element(parser, CHARON_NODE_KIND_STMT_RETURN);
}

static void parse_if(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_IF);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);
    parse_expr(parser);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
    parse_stmt(parser);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_KEYWORD_ELSE)) parse_stmt(parser);

    parser_close_element(parser, CHARON_NODE_KIND_STMT_IF);
}

static void parse_while(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_WHILE);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT)) {
        parse_expr(parser);
        parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
    }
    parse_stmt(parser);

    parser_close_element(parser, CHARON_NODE_KIND_STMT_WHILE);
}

static void parse_for(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_FOR);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);
    if(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON)) {
        parse_declaration(parser);
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    }
    if(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON)) {
        parse_expr(parser);
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    }
    if(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT)) {
        parse_expr(parser);
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
    }
    parse_stmt(parser);

    parser_close_element(parser, CHARON_NODE_KIND_STMT_FOR);
}

static void parse_switch(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_SWITCH);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);
    parse_expr(parser);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
    while(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
        if(parser_consume_try(parser, CHARON_TOKEN_KIND_KEYWORD_DEFAULT)) {
            parser_consume(parser, CHARON_TOKEN_KIND_PNCT_THICK_ARROW);
            parse_stmt(parser);
            continue;
        }

        parse_expr(parser);
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_THICK_ARROW);
        parse_stmt(parser);
    }

    parser_close_element(parser, CHARON_NODE_KIND_STMT_SWITCH);
}

void parse_stmt_block(charon_parser_t *parser) {
    parser_syncset_t prev_syncset = parser->syncset;
    parser->syncset.token_kinds[CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT] = true;

    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
    while(!parser_is_eof(parser) && !parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
        parse_stmt(parser);
    }

    parser_close_element(parser, CHARON_NODE_KIND_STMT_BLOCK);

    parser->syncset = prev_syncset;
}

static void parse_continue(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_CONTINUE);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);

    parser_close_element(parser, CHARON_NODE_KIND_STMT_CONTINUE);
}

static void parse_break(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_KEYWORD_BREAK);
    parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);

    parser_close_element(parser, CHARON_NODE_KIND_STMT_BREAK);
}

void parse_stmt(charon_parser_t *parser) {
    parser_open_element(parser);

    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_PNCT_SEMI_COLON:  parser_consume(parser, CHARON_TOKEN_KIND_PNCT_SEMI_COLON); break;
        case CHARON_TOKEN_KIND_KEYWORD_IF:       parse_if(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_WHILE:    parse_while(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_FOR:      parse_for(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_SWITCH:   parse_switch(parser); break;
        case CHARON_TOKEN_KIND_PNCT_BRACE_LEFT:  parse_stmt_block(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_RETURN:   parse_return(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_LET:      parse_declaration(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_CONTINUE: parse_continue(parser); break;
        case CHARON_TOKEN_KIND_KEYWORD_BREAK:    parse_break(parser); break;
        default:                                 parse_expression(parser); break;
    }

    parser_close_element(parser, CHARON_NODE_KIND_STMT);
}
