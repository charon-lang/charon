#pragma once
#include "../lexer/tokenizer.h"
#include "../ir/node.h"

ir_node_t *parser_parse(tokenizer_t *tokenizer);