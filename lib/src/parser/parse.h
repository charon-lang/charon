#pragma once

#include "parser/parser.h"

void parse_type(parser_t *parser);
void parse_type_function(parser_t *parser);
void parse_expr(parser_t *parser);
void parse_stmt(parser_t *parser);
void parse_stmt_block(parser_t *parser);
void parse_tlc(parser_t *parser);
void parse_root(parser_t *parser);
