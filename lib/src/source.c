#include "source.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

charon_source_t *charon_source_make(const char *name, const char *text, size_t text_length) {
    assert(name != nullptr);
    assert(text != nullptr);
    assert(text_length > 0);

    charon_source_t *source = malloc(sizeof(charon_source_t));
    source->name = strdup(name);
    source->text_length = text_length;

    source->text = malloc(text_length + 1);
    memcpy(source->text, text, text_length);
    source->text[text_length] = '\0';

    return source;
}

void charon_source_destroy(charon_source_t *source) {
    assert(source != nullptr);

    free(source->name);
    free(source->text);
    free(source);
}
