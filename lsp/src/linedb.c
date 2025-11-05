#include "linedb.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void add_line(linedb_t *db, size_t line_start) {
    db->line_starts = reallocarray(db->line_starts, ++db->line_count, sizeof(size_t));
    db->line_starts[db->line_count - 1] = line_start;
}

static size_t find_line(const linedb_t *db, size_t offset) {
    size_t low = 0, high = db->line_count;
    while(low + 1 < high) {
        size_t middle = (low + high) / 2;
        if(offset >= db->line_starts[middle]) {
            low = middle;
            continue;
        }
        high = middle;
    }
    return low;
}

void linedb_build(linedb_t *db, const char *text, size_t text_length) {
    db->line_starts = nullptr;
    db->line_count = 0;

    add_line(db, 0);
    for(size_t i = 0; i < text_length; i++) {
        if(text[i] != '\n') continue;
        add_line(db, i + 1);
    }
}

void linedb_clear(linedb_t *db) {
    free(db->line_starts);
    db->line_starts = nullptr;
    db->line_count = 0;
}

bool linedb_offset_to_position(const linedb_t *db, size_t offset, size_t *out_line, size_t *out_column) {
    if(db->line_count == 0) return false;
    size_t line = find_line(db, offset);
    *out_line = line;
    *out_column = offset - db->line_starts[line];
    return true;
}

bool linedb_position_to_offset(const linedb_t *db, size_t line, size_t column, size_t *out_offset) {
    if(line >= db->line_count) return false;
    *out_offset = db->line_starts[line] + column;
    return true;
}
