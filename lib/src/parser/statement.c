#include "charon/element.h"
#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "parse.h"
#include "parser/collector.h"
#include "parser/parser.h"

static charon_element_inner_t *parse_expression(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    collector_push(&collector, parse_expr(parser));
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    return parser_build(parser, CHARON_NODE_KIND_STMT_EXPRESSION, &collector);
}

static charon_element_inner_t *parse_declaration(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_LET);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COLON)) collector_push(&collector, parse_type(parser));
    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_EQUAL)) collector_push(&collector, parse_expr(parser));

    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    return parser_build(parser, CHARON_NODE_KIND_STMT_DECLARATION, &collector);
}

static charon_element_inner_t *parse_return(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_RETURN);

    if(parser_peek(parser) != CHARON_TOKEN_KIND_PNCT_SEMI_COLON) collector_push(&collector, parse_expr(parser));

    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    return parser_build(parser, CHARON_NODE_KIND_STMT_RETURN, &collector);
}

static charon_element_inner_t *parse_if(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_IF);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);
    collector_push(&collector, parse_expr(parser));
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
    collector_push(&collector, parse_stmt(parser));

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_ELSE)) collector_push(&collector, parse_stmt(parser));

    return parser_build(parser, CHARON_NODE_KIND_STMT_IF, &collector);
}

static charon_element_inner_t *parse_while(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_WHILE);

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT)) {
        collector_push(&collector, parse_expr(parser));
        parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
    }
    collector_push(&collector, parse_stmt(parser));

    return parser_build(parser, CHARON_NODE_KIND_STMT_WHILE, &collector);
}

static charon_element_inner_t *parse_for(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_FOR);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);

    if(!parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON)) {
        collector_push(&collector, parse_declaration(parser));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    }

    if(!parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON)) {
        collector_push(&collector, parse_expr(parser));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    }

    if(!parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT)) {
        collector_push(&collector, parse_expr(parser));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
    }

    collector_push(&collector, parse_stmt(parser));

    return parser_build(parser, CHARON_NODE_KIND_STMT_FOR, &collector);
}

static charon_element_inner_t *parse_switch(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_SWITCH);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);
    collector_push(&collector, parse_expr(parser));
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);

    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
    while(!parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
        if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_DEFAULT)) {
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_THICK_ARROW);
            collector_push(&collector, parse_stmt(parser));
            continue;
        }

        collector_push(&collector, parse_expr(parser));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_THICK_ARROW);
        collector_push(&collector, parse_stmt(parser));
    }

    return parser_build(parser, CHARON_NODE_KIND_STMT_SWITCH, &collector);
}

static charon_element_inner_t *parse_block(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);

    while(!parser_is_eof(parser) && !parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
        collector_push(&collector, parse_stmt(parser));
    }

    return parser_build(parser, CHARON_NODE_KIND_STMT_BLOCK, &collector);
}

static charon_element_inner_t *parse_continue(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_CONTINUE);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    return parser_build(parser, CHARON_NODE_KIND_STMT_CONTINUE, &collector);
}

static charon_element_inner_t *parse_break(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_BREAK);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    return parser_build(parser, CHARON_NODE_KIND_STMT_BREAK, &collector);
}

charon_element_inner_t *parse_stmt(charon_parser_t *parser) {
    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_PNCT_SEMI_COLON: {
            collector_t collector = COLLECTOR_INIT;
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
            return parser_build(parser, CHARON_NODE_KIND_STMT_EMPTY, &collector);
        }
        case CHARON_TOKEN_KIND_KEYWORD_IF:       return parse_if(parser);
        case CHARON_TOKEN_KIND_KEYWORD_WHILE:    return parse_while(parser);
        case CHARON_TOKEN_KIND_KEYWORD_FOR:      return parse_for(parser);
        case CHARON_TOKEN_KIND_KEYWORD_SWITCH:   return parse_switch(parser);
        case CHARON_TOKEN_KIND_PNCT_BRACE_LEFT:  return parse_block(parser);
        case CHARON_TOKEN_KIND_KEYWORD_RETURN:   return parse_return(parser);
        case CHARON_TOKEN_KIND_KEYWORD_LET:      return parse_declaration(parser);
        case CHARON_TOKEN_KIND_KEYWORD_CONTINUE: return parse_continue(parser);
        case CHARON_TOKEN_KIND_KEYWORD_BREAK:    return parse_break(parser);
        default:                                 return parse_expression(parser);
    }
}
