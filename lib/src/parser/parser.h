#pragma once

#include "charon/syntax/node.h"
#include "charon/syntax/token.h"
#include "common/list.h"
#include "core/db.h"
#include "core/diag.h"
#include "lexer_pipeline.h"
#include "syntax/element.h"

#include <stdarg.h>
#include <stddef.h>

typedef struct parser_event parser_event_t;

typedef struct {
    const element_inner_t *root;
    diag_item_t *diagnostics;
} parser_output_t;

typedef struct {
    bool token_kinds[CHARON_TOKEN_KIND_COUNT];
} parser_syncset_t;

typedef struct parser {
    db_t *db;

    lexer_pipeline_t *lexer_pipeline;

    parser_syncset_t syncset;
    list_t events;
} parser_t;

parser_output_t parser_parse_stmt(parser_t *parser);
parser_output_t parser_parse_stmt_block(parser_t *parser);
parser_output_t parser_parse_root(parser_t *parser);


parser_t *parser_make(db_t *db, lexer_pipeline_t *lexer_pipeline);
void parser_destroy(parser_t *parser);

bool parser_is_eof(parser_t *parser);
charon_token_kind_t parser_peek(parser_t *parser);

void parser_consume(parser_t *parser, charon_token_kind_t kind);
void parser_consume_many(parser_t *parser, size_t count, ...);

bool parser_consume_try(parser_t *parser, charon_token_kind_t kind);
bool parser_consume_try_many(parser_t *parser, size_t count, ...);
bool parser_consume_try_many_list(parser_t *parser, size_t count, va_list list);

parser_event_t *parser_checkpoint(parser_t *parser);
void parser_open_element_at(parser_t *parser, parser_event_t *checkpoint);
void parser_open_element(parser_t *parser);
void parser_close_element(parser_t *parser, charon_node_kind_t kind);
void parser_error(parser_t *parser, diag_t diag_kind, diag_data_t *diag_kind_data);

parser_output_t parser_build(parser_t *parser);
