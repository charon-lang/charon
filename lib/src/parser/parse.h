#pragma once

#include "charon/parser.h"

void parse_type(charon_parser_t *parser);
void parse_type_function(charon_parser_t *parser);
void parse_expr(charon_parser_t *parser);
void parse_stmt(charon_parser_t *parser);
void parse_stmt_block(charon_parser_t *parser);
void parse_tlc(charon_parser_t *parser);
void parse_root(charon_parser_t *parser);
