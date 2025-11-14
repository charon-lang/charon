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

/**
 * Creates a slice of utf8 text based on a binary offset and size.
 * For utf8 codepoint indexes use the substring functions.
 */
utf8_slice_t utf8_slice(const charon_utf8_text_t *text, size_t start_index, size_t size);

/**
 * Creates a malloced copy of a slice.
 */
charon_utf8_text_t *utf8_slice_copy(utf8_slice_t slice);

/**
 * Create a substring from a utf8 codepoint index to the end of the text.
 */
utf8_slice_t utf8_substring_end(const charon_utf8_text_t *text, size_t start_char);

/**
 * Create a substring from the start of the text to the end of a utf8 codepoint index.
 */
utf8_slice_t utf8_substring_start(const charon_utf8_text_t *text, size_t end_char);
