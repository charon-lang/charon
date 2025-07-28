#include "diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>

#define INFO_LINE_COUNT 3

typedef struct {
    bool present;
    size_t offset, length;
} line_t;

static const char *g_strings[] = {
    [LANG_MISSING] = "Missing language translation",
    #define STRING(ID, VALUE) [LANG_##ID] = VALUE,
    #include "lang_strings.def"
    #undef STRING
};

static void diag(source_location_t *source_location, lang_t fmt, va_list list, char *type, FILE *fd) {
    size_t x = 0, y = 0;
    line_t lines[INFO_LINE_COUNT] = {
        { .present = true }
    };

    for(size_t i = 0; i < source_location->offset; i++) {
        x++;
        if(source_location->source->data_buffer[i] != '\n') continue;
        x = 0;
        y++;
        for(size_t j = INFO_LINE_COUNT - 1; j >= 1; j--) lines[j] = lines[j - 1];
        lines[0] = (line_t) { .present = true, .offset = i + 1};
    }
    for(size_t i = 0; i < INFO_LINE_COUNT; i++) {
        if(!lines[i].present) continue;
        for(size_t j = lines[i].offset; j < source_location->source->data_buffer_size; j++) {
            if(source_location->source->data_buffer[j] == '\n') break;
            lines[i].length++;
        }
    }

    fprintf(fd, "\e[1m%s:%lu:%lu\e[0m %s: \e[0m", source_location->source->name, y + 1, x + 1, type);
    vfprintf(fd, g_strings[fmt], list);
    fprintf(fd, "\n");

    for(size_t i = INFO_LINE_COUNT; i > 0; i--) {
        if(!lines[i - 1].present) continue;
        fprintf(fd, "%.*s\n", (int) lines[i - 1].length, &source_location->source->data_buffer[lines[i - 1].offset]);
    }
    fprintf(fd, "%*s^\n", (int) (source_location->offset - lines[0].offset), "");
}

[[noreturn]] void diag_error(source_location_t source_location, lang_t fmt, ...) {
    va_list list;
    va_start(list, fmt);
    diag(&source_location, fmt, list, "\e[91merror", stderr);
    va_end(list);
    exit(EXIT_FAILURE);
}

void diag_warn(source_location_t source_location, lang_t fmt, ...) {
    va_list list;
    va_start(list, fmt);
    diag(&source_location, fmt, list, "\e[93mwarn", stderr);
    va_end(list);
}
