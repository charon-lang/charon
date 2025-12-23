#include "charon/diag.h"

#include "charon/token.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static char *append(char *original, const char *fmt, const char *new) {
    char *tmp;
    assert(asprintf(&tmp, fmt, original, new) > 0);
    free(original);
    return tmp;
}

static char *token_kinds_string(size_t count, charon_token_kind_t *kinds) {
    char *str = "";
    for(size_t i = 0; i < count; i++) {
        const char *kind_str = charon_token_kind_tostring(kinds[i]);
        if(i == 0) {
            str = strdup(kind_str);
        } else {
            str = append(str, "%s, %s", kind_str);
        }
    }
    return str;
}

const char *charon_diag_tostring(charon_diag_t diag) {
    static const char *translations[] = {
#define DIAGNOSTIC(ID, NAME, ...) [CHARON_DIAG_##ID] = NAME,
#include "charon/diagnostics.def"
#undef DIAGNOSTIC
    };
    return translations[diag];
}

char *charon_diag_fmt(charon_diag_t diag, charon_diag_data_t *data) {
    switch(diag) {
        case CHARON_DIAG_UNEXPECTED_TOKEN: {
            char *str = token_kinds_string(data->unexpected_token.expected_count, data->unexpected_token.expected);
            return append(str, "Expected %s got %s", charon_token_kind_tostring(data->unexpected_token.found));
        };
    }
    assert(false);
}
