#pragma once

#include "charon/element.h"
#include "charon/utf8.h"

#include <stddef.h>

typedef struct {
    const char *name;
    const charon_utf8_text_t *text;
    const charon_element_inner_t *root_element;
} charon_file_t;
