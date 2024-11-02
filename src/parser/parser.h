#pragma once

#include "hlir/node.h"
#include "lexer/tokenizer.h"

hlir_node_t *parser_root(tokenizer_t *tokenizer);
hlir_node_t *parser_tlc(tokenizer_t *tokenizer);
/** @warning can return null */
hlir_node_t *parser_stmt(tokenizer_t *tokenizer);
hlir_node_t *parser_expr(tokenizer_t *tokenizer);