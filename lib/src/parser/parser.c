#include "parser.h"

#include "charon/element.h"
#include "charon/lexer.h"
#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "parser/collector.h"
#include "parser/parse.h"

#include <stdarg.h>

charon_parser_t *charon_parser_make(charon_element_cache_t *element_cache, charon_lexer_t *lexer) {
    charon_parser_t *parser = malloc(sizeof(charon_parser_t));
    parser->cache = element_cache;
    parser->lexer = lexer;
    return parser;
}

void charon_parser_destroy(charon_parser_t *parser) {
    free(parser);
}

charon_element_inner_t *charon_parser_parse_type(charon_parser_t *parser) {
    return parse_type(parser);
}

charon_element_inner_t *charon_parser_parse_expr(charon_parser_t *parser) {
    return parse_expr(parser);
}

charon_element_inner_t *charon_parser_parse_stmt(charon_parser_t *parser) {
    return parse_stmt(parser);
}

charon_element_inner_t *charon_parser_parse_tlc(charon_parser_t *parser) {
    return parse_tlc(parser);
}

charon_element_inner_t *charon_parser_parse_root(charon_parser_t *parser) {
    return parse_root(parser);
}

charon_token_kind_t parser_peek(charon_parser_t *parser) {
    return charon_element_inner_token_kind(charon_lexer_peek(parser->lexer));
}

void parser_consume(charon_parser_t *parser, collector_t *collector, charon_token_kind_t kind) {
    if(parser_consume_try(parser, collector, kind)) return;

    collector_t error_collector = COLLECTOR_INIT;
    parser_consume_any(parser, &error_collector);
    charon_element_inner_t *error_elem = parser_build(parser, CHARON_NODE_KIND_ERROR, &error_collector);

    collector_push(collector, error_elem);
}

void parser_consume_any(charon_parser_t *parser, collector_t *collector) {
    collector_push(collector, charon_lexer_advance(parser->lexer));
}

bool parser_consume_try(charon_parser_t *parser, collector_t *collector, charon_token_kind_t kind) {
    if(parser_peek(parser) == kind) {
        parser_consume_any(parser, collector);
        return true;
    }
    return false;
}

bool parser_consume_oneof(charon_parser_t *parser, collector_t *collector, size_t kind_count, ...) {
    va_list list;
    va_start(list, kind_count);

    charon_token_kind_t kind = parser_peek(parser);
    for(size_t i = 0; i < kind_count; i++) {
        charon_token_kind_t valid_kind = va_arg(list, charon_token_kind_t);
        if(kind == valid_kind) {
            va_end(list);
            parser_consume_any(parser, collector);
            return true;
        }
    }

    va_end(list);
    return false;
}

charon_element_inner_t *parser_build(charon_parser_t *parser, charon_node_kind_t kind, collector_t *collector) {
    charon_element_inner_t *element = charon_element_inner_make_node(parser->cache, kind, collector->elements, collector->count);
    collector_clear(collector);
    return element;
}
