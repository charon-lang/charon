#pragma once

#include "charon/utf8.h"

#include <stddef.h>
#include <stdint.h>

struct charon_utf8_text {
    size_t size;
    uint8_t data[];
};

typedef struct {
    size_t start_index, size;
    const charon_utf8_text_t *text;
} utf8_slice_t;

utf8_slice_t utf8_slice(const charon_utf8_text_t *text, size_t start_index, size_t size);
charon_utf8_text_t *utf8_slice_copy(utf8_slice_t slice);
