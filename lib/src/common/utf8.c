#include "utf8.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

// static size_t effective_size(const charon_text_t *text) {
//     const int8_t *base = UTF8_DATA(text);

//     const int8_t *str = base;
//     while((size_t) (str - base) < text->size && '\0' != *str) {
//         size_t advance = lead_width(*str);
//         size_t consumed = (size_t) (str - base);
//         size_t remaining = text->size - consumed;
//         if(advance > remaining) {
//             str = base + text->size;
//             break;
//         }
//         str += advance;
//     }

//     if((size_t) (str - base) > text->size) str = base + text->size;

//     return (size_t) (str - base);
// }

// static size_t byte_offset(const charon_text_t *text, size_t char_offset) {
//     const int8_t *base = UTF8_DATA(text);

//     size_t current = 0;
//     const int8_t *str = base;
//     while(current < char_offset && (size_t) (str - base) < text->size && '\0' != *str) {
//         size_t advance = lead_width(*str);
//         size_t consumed = (size_t) (str - base);
//         size_t remaining = text->size - consumed;
//         if(advance > remaining) {
//             str = base + text->size;
//             break;
//         }
//         str += advance;
//         current++;
//     }

//     if((size_t) (str - base) > text->size) return text->size;

//     return (size_t) (str - base);
// }

// static size_t utf8_character_size(const charon_text_t *text, size_t byte_index) {
//     return lead_width(UTF8_DATA(text)[byte_index]);
// }

// static size_t utf8_length(const charon_text_t *text) {
//     if(text == nullptr) return 0;

//     const int8_t *str = UTF8_DATA(text);
//     const int8_t *base = str;
//     size_t count = 0;

//     while((size_t) (str - base) < text->size && '\0' != *str) {
//         size_t advance = lead_width(*str);
//         size_t consumed = (size_t) (str - base);
//         size_t remaining = text->size - consumed;
//         if(advance > remaining) break;

//         str += advance;
//         count++;
//     }

//     return count;
// }

// static char *utf8_to_ascii(const charon_text_t *text) {
//     char *ascii = malloc(text->size + 1);
//     memcpy(ascii, TEXT_DATA(text), text->size);
//     ascii[text->size] = '\0';
//     return ascii;
// }

// static int utf8_strcmp(const charon_text_t *lhs, const charon_text_t *rhs) {
//     size_t lhs_size = effective_size(lhs);
//     size_t rhs_size = effective_size(rhs);
//     size_t min_size = lhs_size < rhs_size ? lhs_size : rhs_size;

//     int cmp = memcmp(UTF8_DATA(lhs), UTF8_DATA(rhs), min_size);
//     if(cmp != 0) return cmp;
//     if(lhs_size == rhs_size) return 0;
//     return lhs_size < rhs_size ? -1 : 1;
// }

// static charon_text_t *utf8_substr_end(const charon_text_t *text, size_t start_char) {
//     if(text == nullptr) return nullptr;

//     size_t start_offset = byte_offset(text, start_char);
//     size_t size = effective_size(text);
//     if(start_offset > size) start_offset = size;

//     size_t slice_length = size - start_offset;
//     return text_slice(text, start_offset, slice_length);
// }

// static charon_text_t *utf8_substr_start(const charon_text_t *text, size_t end_char) {
//     if(text == nullptr) return nullptr;

//     size_t end_offset = byte_offset(text, end_char);
//     size_t size = effective_size(text);
//     if(end_offset > size) end_offset = size;

//     return text_slice(text, 0, end_offset);
// }
