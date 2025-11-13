#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct charon_utf8_text charon_utf8_text_t;

size_t charon_utf8_lead_width(uint8_t ch);

charon_utf8_text_t *charon_utf8_from(const char *data, size_t data_length);

const char *charon_utf8_as_string(const charon_utf8_text_t *text);
