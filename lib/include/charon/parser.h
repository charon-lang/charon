#pragma once

#include "charon/diag.h"
#include "charon/element.h"
#include "charon/lexer.h"

typedef struct charon_parser charon_parser_t;

typedef struct {
    const charon_element_inner_t *root;
    charon_diag_item_t *diagnostics;
} charon_parser_output_t;

charon_parser_t *charon_parser_make(charon_element_cache_t *element_cache, charon_lexer_t *lexer);
void charon_parser_destroy(charon_parser_t *parser);

charon_parser_output_t charon_parser_parse_stmt(charon_parser_t *parser);
charon_parser_output_t charon_parser_parse_stmt_block(charon_parser_t *parser);
charon_parser_output_t charon_parser_parse_root(charon_parser_t *parser);
