#include "parser.h"

#include "charon/element.h"
#include "charon/lexer.h"
#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "parser/collector.h"
#include "parser/parse.h"

#include <stdarg.h>
#include <stddef.h>

static void raw_consume(charon_parser_t *parser, collector_t *collector) {
    collector_push(collector, charon_lexer_advance(parser->lexer));
}

charon_parser_t *charon_parser_make(charon_element_cache_t *element_cache, charon_lexer_t *lexer) {
    charon_parser_t *parser = malloc(sizeof(charon_parser_t));
    parser->cache = element_cache;
    parser->lexer = lexer;
    for(size_t i = 0; i < CHARON_TOKEN_KIND_COUNT; i++) parser->syncset.token_kinds[i] = false;
    return parser;
}

void charon_parser_destroy(charon_parser_t *parser) {
    free(parser);
}

const charon_element_inner_t *charon_parser_parse_stmt(charon_parser_t *parser) {
    return parse_stmt(parser);
}

const charon_element_inner_t *charon_parser_parse_stmt_block(charon_parser_t *parser) {
    return parse_stmt_block(parser);
}

const charon_element_inner_t *charon_parser_parse_root(charon_parser_t *parser) {
    return parse_root(parser);
}

charon_token_kind_t parser_peek(charon_parser_t *parser) {
    return charon_element_token_kind(charon_lexer_peek(parser->lexer));
}

void parser_consume(charon_parser_t *parser, collector_t *collector, charon_token_kind_t kind) {
    if(parser_consume_try(parser, collector, kind)) return;
    collector_push(collector, parser_build_unexpected_error(parser));
}

void parser_consume_any(charon_parser_t *parser, collector_t *collector) {
    if(parser->syncset.token_kinds[parser_peek(parser)]) {
        collector_push(collector, parser_build_unexpected_error(parser));
    } else {
        raw_consume(parser, collector);
    }
}

bool parser_consume_try(charon_parser_t *parser, collector_t *collector, charon_token_kind_t kind) {
    if(parser_peek(parser) == kind) {
        raw_consume(parser, collector);
        return true;
    }
    return false;
}

const charon_element_inner_t *parser_build_unexpected_error(charon_parser_t *parser) {
    collector_t error_collector = COLLECTOR_INIT;
    if(!parser->syncset.token_kinds[parser_peek(parser)]) {
        raw_consume(parser, &error_collector);
    }
    return parser_build(parser, CHARON_NODE_KIND_ERROR, &error_collector);
}

const charon_element_inner_t *parser_build(charon_parser_t *parser, charon_node_kind_t kind, collector_t *collector) {
    const charon_element_inner_t *element = charon_element_inner_make_node(parser->cache, kind, collector->elements, collector->count);
    collector_clear(collector);
    return element;
}
