#include "utf8.h"

#include "charon/utf8.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t byte_offset(const charon_utf8_text_t *text, size_t char_offset) {
    size_t index = 0;
    size_t char_count = 0;
    while(char_count < char_offset && index < text->size && text->data[index] != '\0') {
        size_t advance = charon_utf8_lead_width(text->data[index]);
        if(advance > text->size - index) {
            index = text->size;
            break;
        }
        index += advance;
        char_count++;
    }
    assert(index <= text->size);
    return index;
}

charon_utf8_text_t *charon_utf8_from(const char *data, size_t data_length) {
    charon_utf8_text_t *text = malloc(sizeof(charon_utf8_text_t) + data_length + 1);
    text->size = data_length;
    text->data[data_length] = '\0';
    memcpy(text->data, data, data_length);
    return text;
}

const char *charon_utf8_as_string(const charon_utf8_text_t *text) {
    return (const char *) text->data;
}

charon_utf8_text_t *utf8_copy(const charon_utf8_text_t *text) {
    return utf8_slice_copy((utf8_slice_t) { .text = text, .size = text->size, .start_index = 0 });
}

utf8_slice_t utf8_slice(const charon_utf8_text_t *text, size_t start_index, size_t size) {
    if(start_index > text->size) start_index = text->size;
    if(start_index + size > text->size) size = text->size - start_index;
    return (utf8_slice_t) { .start_index = start_index, .size = size, .text = text };
}

charon_utf8_text_t *utf8_slice_copy(utf8_slice_t slice) {
    charon_utf8_text_t *text = malloc(sizeof(charon_utf8_text_t) + slice.size + 1);
    text->size = slice.size;
    text->data[slice.size] = '\0';
    if(slice.size > 0) memcpy(text->data, &slice.text->data[slice.start_index], slice.size);
    return text;
}

size_t charon_utf8_lead_width(uint8_t ch) {
    if(0xF0 == (0xF8 & ch)) return 4;
    if(0xE0 == (0xF0 & ch)) return 3;
    if(0xC0 == (0xE0 & ch)) return 2;
    return 1;
}

utf8_slice_t utf8_substring_end(const charon_utf8_text_t *text, size_t start_char) {
    size_t start_offset = byte_offset(text, start_char);
    if(start_offset > text->size) start_offset = text->size;

    size_t slice_length = text->size - start_offset;
    return utf8_slice(text, start_offset, slice_length);
}

utf8_slice_t utf8_substring_start(const charon_utf8_text_t *text, size_t end_char) {
    size_t end_offset = byte_offset(text, end_char);
    if(end_offset > text->size) end_offset = text->size;

    return utf8_slice(text, 0, end_offset);
}
