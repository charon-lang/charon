#include "charon/element.h"
#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "parse.h"
#include "parser/collector.h"
#include "parser/parser.h"

static charon_element_inner_t *parse_type_definition(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_TYPE);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_CARET_LEFT)) {
        do {
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);
        } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT);
    }

    collector_push(&collector, parse_type(parser));

    return parser_build(parser, CHARON_NODE_KIND_TLC_TYPE_DEFINITION, &collector);
}

static charon_element_inner_t *parse_module(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_MODULE);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);

    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
    while(!parser_is_eof(parser) && !parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
        collector_push(&collector, parse_tlc(parser));
    }

    return parser_build(parser, CHARON_NODE_KIND_TLC_MODULE, &collector);
}

static charon_element_inner_t *parse_function(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_FUNCTION);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_CARET_LEFT)) {
        do {
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);
        } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT);
    }

    collector_push(&collector, parse_type_function(parser));

    collector_push(&collector, parse_stmt(parser));

    return parser_build(parser, CHARON_NODE_KIND_TLC_FUNCTION, &collector);
}

static charon_element_inner_t *parse_extern(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_EXTERN);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_FUNCTION);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);

    collector_push(&collector, parse_type_function(parser));

    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    return parser_build(parser, CHARON_NODE_KIND_TLC_EXTERN, &collector);
}

static charon_element_inner_t *parse_declaration(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_LET);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);

    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_COLON);
    collector_push(&collector, parse_type(parser));

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_EQUAL)) collector_push(&collector, parse_expr(parser));

    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_SEMI_COLON);
    return parser_build(parser, CHARON_NODE_KIND_STMT_DECLARATION, &collector);
}

static charon_element_inner_t *parse_enum(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_ENUM);
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);

    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
    if(parser_peek(parser) == CHARON_TOKEN_KIND_IDENTIFIER) {
        do {
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);
        } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
    }
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT);

    return parser_build(parser, CHARON_NODE_KIND_TLC_ENUMERATION, &collector);
}

charon_element_inner_t *parse_tlc(charon_parser_t *parser) {
    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_KEYWORD_MODULE:   return parse_module(parser);
        case CHARON_TOKEN_KIND_KEYWORD_FUNCTION: return parse_function(parser);
        case CHARON_TOKEN_KIND_KEYWORD_EXTERN:   return parse_extern(parser);
        case CHARON_TOKEN_KIND_KEYWORD_TYPE:     return parse_type_definition(parser);
        case CHARON_TOKEN_KIND_KEYWORD_LET:      return parse_declaration(parser);
        case CHARON_TOKEN_KIND_KEYWORD_ENUM:     return parse_enum(parser);
        default:                                 {
            collector_t collector = COLLECTOR_INIT;
            parser_consume_any(parser, &collector);
            return parser_build(parser, CHARON_NODE_KIND_ERROR, &collector);
        }
    }
}
