#pragma once

#include "parser.h"

charon_element_inner_t *parse_type(charon_parser_t *parser);
charon_element_inner_t *parse_type_function(charon_parser_t *parser);
charon_element_inner_t *parse_expr(charon_parser_t *parser);
charon_element_inner_t *parse_stmt(charon_parser_t *parser);
charon_element_inner_t *parse_tlc(charon_parser_t *parser);
charon_element_inner_t *parse_root(charon_parser_t *parser);
