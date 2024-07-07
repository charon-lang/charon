#pragma once

#include "ir/node.h"
#include "lexer/tokenizer.h"

ir_node_t *parser_root(tokenizer_t *tokenizer);
ir_node_t *parser_tlc(tokenizer_t *tokenizer);
ir_node_t *parser_stmt(tokenizer_t *tokenizer);
ir_node_t *parser_expr(tokenizer_t *tokenizer);