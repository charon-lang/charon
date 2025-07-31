#include "diag.h"

#include "charon/diag.h"
#include "lib/context.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

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

static void print_diag(source_location_t *source_location, lang_t fmt, va_list list, char *type, FILE *fd) {
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
    print_diag(&source_location, fmt, list, "\e[91merror", stderr);
    va_end(list);
    exit(EXIT_FAILURE);
}

void diag_warn(source_location_t source_location, lang_t fmt, ...) {
    va_list list;
    va_start(list, fmt);
    print_diag(&source_location, fmt, list, "\e[93mwarn", stderr);
    va_end(list);
}

char *charon_diag_tostring(charon_diag_t *diag) {
    char *str = NULL;
    switch(diag->type) {
        case DIAG_TYPE__UNEXPECTED_SYMBOL:           asprintf(&str, "unexpected symbol"); break;
        case DIAG_TYPE__UNFINISHED_MODULE:           asprintf(&str, "unexpected end of module `%s`", diag->data.unfinished_module_name); break;
        case DIAG_TYPE__EXPECTED:                    asprintf(&str, "expected `%s` got `%s`", token_kind_stringify(diag->data.expected.expected), token_kind_stringify(diag->data.expected.actual)); break;
        case DIAG_TYPE__EXPECTED_STATEMENT:          asprintf(&str, "expected statement"); break;
        case DIAG_TYPE__EXPECTED_BINARY_OPERATION:   asprintf(&str, "expected binary operation"); break;
        case DIAG_TYPE__EXPECTED_PRIMARY_EXPRESSION: asprintf(&str, "expected primary expression"); break;
        case DIAG_TYPE__EXPECTED_TLC:                asprintf(&str, "expected top level construct"); break;
        case DIAG_TYPE__EXPECTED_NUMERIC_LITERAL:    asprintf(&str, "expected numeric literal"); break;
        case DIAG_TYPE__EXPECTED_ATTRIBUTE_ARGUMENT: asprintf(&str, "expected attribute argument"); break;
        case DIAG_TYPE__EMPTY_CHAR_LITERAL:          asprintf(&str, "character literal is empty"); break;
        case DIAG_TYPE__TOO_LARGE_CHAR_LITERAL:      asprintf(&str, "character literal exceeds maximum size"); break;
        case DIAG_TYPE__TOO_LARGE_ESCAPE_SEQUENCE:   asprintf(&str, "escape sequence exceeds maximum size"); break;
        case DIAG_TYPE__TOO_LARGE_NUMERIC_CONSTANT:  asprintf(&str, "numeric constant exceeds maximum value"); break;
        case DIAG_TYPE__DUPLICATE_DEFAULT:           asprintf(&str, "duplicate definition of default"); break;
    }
    return str;
}

diag_t *diag(source_location_t source_location, diag_t diag) {
    assert(g_global_context != NULL);

    diag_item_t *item = malloc(sizeof(diag_item_t));
    item->location = source_location;
    item->diagnostic = diag;
    vector_diag_item_append(&g_global_context->diag_items, item);
    return &item->diagnostic;
}
