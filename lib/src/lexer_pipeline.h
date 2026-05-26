#pragma once

#include "charon/lexer.h"
#include "syntax/element.h"

typedef struct lexer_pipeline lexer_pipeline_t;

lexer_pipeline_t *lexer_pipeline_make(db_t *db, charon_lexer_t *lexer);
void lexer_pipeline_destroy(lexer_pipeline_t *pipeline);

const element_inner_t *lexer_pipeline_peek(lexer_pipeline_t *pipeline);
const element_inner_t *lexer_pipeline_advance(lexer_pipeline_t *pipeline);
bool lexer_pipeline_is_eof(lexer_pipeline_t *pipeline);
