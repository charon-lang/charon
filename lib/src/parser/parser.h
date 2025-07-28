#pragma once

#include "lexer/tokenizer.h"
#include "ast/node.h"

ast_node_t *parser_root(tokenizer_t *tokenizer);
ast_node_t *parser_tlc(tokenizer_t *tokenizer);
/** @warning can return null */
ast_node_t *parser_stmt(tokenizer_t *tokenizer);
ast_node_t *parser_expr(tokenizer_t *tokenizer);
