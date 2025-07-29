#include "diagnostic.h"

#include <stdio.h>
#include <stdlib.h>

#define INFO_LINE_COUNT 3

typedef struct {
    bool present;
    size_t offset, length;
} line_t;

void diagnostic_print(source_location_t *source_location, charon_diag_t diag) {
    size_t x = 0, y = 0;
    line_t lines[INFO_LINE_COUNT] = { { .present = true } };

    for(size_t i = 0; i < source_location->offset; i++) {
        x++;
        if(source_location->source->data_buffer[i] != '\n') continue;
        x = 0;
        y++;
        for(size_t j = INFO_LINE_COUNT - 1; j >= 1; j--) lines[j] = lines[j - 1];
        lines[0] = (line_t) { .present = true, .offset = i + 1 };
    }
    for(size_t i = 0; i < INFO_LINE_COUNT; i++) {
        if(!lines[i].present) continue;
        for(size_t j = lines[i].offset; j < source_location->source->data_buffer_size; j++) {
            if(source_location->source->data_buffer[j] == '\n') break;
            lines[i].length++;
        }
    }

    fprintf(stderr, "\e[1m%s:%lu:%lu\e[0m %s: \e[0m", source_location->source->name, y + 1, x + 1, "\e[91merror");
    char *str = charon_diag_tostring(&diag);
    fprintf(stderr, "%s\n", str);
    free(str);

    for(size_t i = INFO_LINE_COUNT; i > 0; i--) {
        if(!lines[i - 1].present) continue;
        fprintf(stderr, "%.*s\n", (int) lines[i - 1].length, &source_location->source->data_buffer[lines[i - 1].offset]);
    }
    fprintf(stderr, "%*s^\n\n", (int) (source_location->offset - lines[0].offset), "");
}
