#pragma once

#include "charon/element.h"
#include "charon/lexer.h"
#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "collector.h"

#include <stddef.h>

typedef struct {
    bool token_kinds[CHARON_TOKEN_KIND_COUNT];
} charon_parser_syncset_t;

struct charon_parser {
    charon_element_cache_t *cache;
    charon_lexer_t *lexer;
    charon_parser_syncset_t syncset;
};

static inline bool parser_is_eof(charon_parser_t *parser) {
    return charon_lexer_is_eof(parser->lexer);
}

charon_token_kind_t parser_peek(charon_parser_t *parser);

void parser_consume(charon_parser_t *parser, collector_t *collector, charon_token_kind_t kind);
void parser_consume_any(charon_parser_t *parser, collector_t *collector);
bool parser_consume_try(charon_parser_t *parser, collector_t *collector, charon_token_kind_t kind);

const charon_element_inner_t *parser_build_unexpected_error(charon_parser_t *parser);
const charon_element_inner_t *parser_build(charon_parser_t *parser, charon_node_kind_t kind, collector_t *collector);
