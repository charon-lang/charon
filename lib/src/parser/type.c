#include "charon/element.h"
#include "charon/node.h"
#include "common/fatal.h"
#include "parser.h"
#include "parser/collector.h"

charon_element_inner_t *parse_type(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;

    // Struct type
    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_STRUCT)) {
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT);
        if(!parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
            do {
                parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);
                parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_COLON);
                collector_push(&collector, parse_type(parser));
            } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT);
        }
        return parser_build(parser, CHARON_NODE_KIND_TYPE_STRUCT, &collector);
    }

    // Tuple type
    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT)) {
        do {
            collector_push(&collector, parse_type(parser));
        } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
        return parser_build(parser, CHARON_NODE_KIND_TYPE_TUPLE, &collector);
    }

    // Array type
    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACKET_LEFT)) {
        collector_push(&collector, parse_type(parser));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACKET_RIGHT);
        return parser_build(parser, CHARON_NODE_KIND_TYPE_ARRAY, &collector);
    }

    // Pointer type
    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_STAR)) {
        collector_push(&collector, parse_type(parser));
        return parser_build(parser, CHARON_NODE_KIND_TYPE_POINTER, &collector);
    }

    // Function reference type
    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_FUNCTION)) {
        // CALL(function_type);
        fatal("unimplemented");
        return parser_build(parser, CHARON_NODE_KIND_TYPE_FUNCTION_REF, &collector);
    }

    // Type reference
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);
    while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_DOUBLE_COLON)) {
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);
    }

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_CARET_LEFT)) {
        do {
            collector_push(&collector, parse_type(parser));
        } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT);
    }

    return parser_build(parser, CHARON_NODE_KIND_TYPE_REFERENCE, &collector);
}
