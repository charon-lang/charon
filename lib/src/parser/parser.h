#pragma once

#include "ast/node.h"
#include "lexer/tokenizer.h"

ast_node_t *parser_root(tokenizer_t *tokenizer);
ast_node_t *parser_tlc(tokenizer_t *tokenizer);
/** @warning can return null */
ast_node_t *parser_top_stmt(tokenizer_t *tokenizer);
ast_node_t *parser_expr(tokenizer_t *tokenizer);
