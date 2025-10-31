#pragma once

#include "charon/element.h"
#include "charon/lexer.h"

typedef struct charon_parser charon_parser_t;

charon_parser_t *charon_parser_make(charon_element_cache_t *element_cache, charon_lexer_t *lexer);
void charon_parser_destroy(charon_parser_t *parser);

charon_element_inner_t *charon_parser_parse_type(charon_parser_t *parser);
charon_element_inner_t *charon_parser_parse_expr(charon_parser_t *parser);
