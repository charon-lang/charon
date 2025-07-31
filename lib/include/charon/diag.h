#pragma once

#include "lexer/token.h"

typedef enum charon_diag_type {
    DIAG_TYPE__UNEXPECTED_SYMBOL,

    DIAG_TYPE__UNFINISHED_MODULE,

    DIAG_TYPE__EXPECTED,
    DIAG_TYPE__EXPECTED_STATEMENT,
    DIAG_TYPE__EXPECTED_BINARY_OPERATION,
    DIAG_TYPE__EXPECTED_PRIMARY_EXPRESSION,
    DIAG_TYPE__EXPECTED_TLC,
    DIAG_TYPE__EXPECTED_NUMERIC_LITERAL,
    DIAG_TYPE__EXPECTED_ATTRIBUTE_ARGUMENT,

    DIAG_TYPE__TOO_LARGE_CHAR_LITERAL,
    DIAG_TYPE__TOO_LARGE_ESCAPE_SEQUENCE,
    DIAG_TYPE__TOO_LARGE_NUMERIC_CONSTANT,

    DIAG_TYPE__EMPTY_CHAR_LITERAL,

    DIAG_TYPE__DUPLICATE_DEFAULT
} charon_diag_type_t;

typedef struct charon_diag {
    charon_diag_type_t type;
    union {
        struct {
            token_kind_t expected;
            token_kind_t actual;
        } expected;
        const char *unfinished_module_name;
    } data;
} charon_diag_t;

char *charon_diag_tostring(charon_diag_t *diag);
