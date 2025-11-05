#pragma once

#include <stddef.h>

typedef struct {
    size_t *line_starts;
    size_t line_count;
} linedb_t;

void linedb_build(linedb_t *db, const char *text, size_t text_length);
void linedb_clear(linedb_t *db);

bool linedb_offset_to_position(const linedb_t *db, size_t offset, size_t *out_line, size_t *out_column);
bool linedb_position_to_offset(const linedb_t *db, size_t line, size_t column, size_t *out_offset);
