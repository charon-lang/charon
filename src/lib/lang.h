#pragma once

typedef enum {
    LANG_MISSING,
    #define STRING(ID, ...) LANG_##ID,
    #include "lang_strings.def"
    #undef STRING
} lang_t;
