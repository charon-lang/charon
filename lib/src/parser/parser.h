#pragma once

#include "charon/diag.h"
#include "charon/element.h"
#include "charon/lexer.h"
#include "charon/node.h"
#include "charon/parser.h"
#include "charon/token.h"
#include "lib/list.h"

#include <stdarg.h>
#include <stddef.h>

typedef struct parser_event parser_event_t;

typedef struct {
    bool token_kinds[CHARON_TOKEN_KIND_COUNT];
} parser_syncset_t;

struct charon_parser {
    charon_element_cache_t *cache;
    charon_lexer_t *lexer;

    parser_syncset_t syncset;
    list_t events;

    charon_diag_t *diagnostics;
};

bool parser_is_eof(charon_parser_t *parser);
charon_token_kind_t parser_peek(charon_parser_t *parser);

void parser_consume(charon_parser_t *parser, charon_token_kind_t kind);
void parser_consume_many(charon_parser_t *parser, size_t count, ...);

bool parser_consume_try(charon_parser_t *parser, charon_token_kind_t kind);
bool parser_consume_try_many(charon_parser_t *parser, size_t count, ...);
bool parser_consume_try_many_list(charon_parser_t *parser, size_t count, va_list list);

parser_event_t *parser_checkpoint(charon_parser_t *parser);
void parser_open_element_at(charon_parser_t *parser, parser_event_t *checkpoint);
void parser_open_element(charon_parser_t *parser);
void parser_close_element(charon_parser_t *parser, charon_node_kind_t kind);
void parser_error(charon_parser_t *parser);

void parser_unexpected_error(charon_parser_t *parser);

const charon_element_inner_t *parser_build(charon_parser_t *parser);
