#include "diag.h"
#include <stdlib.h>
#include <stdio.h>

#define INFO_LINE_COUNT 3

typedef struct {
    bool present;
    size_t offset, length;
} line_t;

static void diag(diag_loc_t *loc, char *fmt, va_list list, char *type, FILE *fd) {
    if(loc->present) {
        size_t x = 0, y = 0;
        line_t lines[INFO_LINE_COUNT] = {
            { .present = true }
        };

        for(size_t i = 0; i < loc->offset; i++) {
            x++;
            if(loc->source->data[i] != '\n') continue;
            x = 0;
            y++;
            for(size_t j = INFO_LINE_COUNT - 1; j >= 1; j--) lines[j] = lines[j - 1];
            lines[0] = (line_t) { .present = true, .offset = i + 1};
        }
        for(size_t i = 0; i < INFO_LINE_COUNT; i++) {
            if(!lines[i].present) continue;
            for(size_t j = lines[i].offset; j < loc->source->data_length; j++) {
                if(loc->source->data[j] == '\n') break;
                lines[i].length++;
            }
        }

        fprintf(fd, "\e[1m%s:%lu:%lu\e[0m %s: \e[0m", loc->source->name, y + 1, x + 1, type);
        vfprintf(fd, fmt, list);
        fprintf(fd, "\n");

        for(size_t i = INFO_LINE_COUNT; i > 0; i--) {
            if(!lines[i - 1].present) continue;
            fprintf(fd, "%.*s\n", lines[i - 1].length, &loc->source->data[lines[i - 1].offset]);
        }
        fprintf(fd, "%*s^\n", loc->offset - lines[0].offset, "");
    } else {
        fprintf(fd, "%s: \e[0m", type);
        vfprintf(fd, fmt, list);
        fprintf(fd, "\n");
    }
}

[[noreturn]] void diag_error(diag_loc_t diag_loc, char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    diag(&diag_loc, fmt, list, "\e[91merror", stderr);
    va_end(list);
    exit(EXIT_FAILURE);
}

void diag_warn(diag_loc_t diag_loc, char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    diag(&diag_loc, fmt, list, "\e[93mwarn", stdout);
    va_end(list);
}