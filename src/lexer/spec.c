#include "spec.h"

#include "lib/log.h"

#include <stdint.h>
#include <stdlib.h>
#include <pcre2.h>

typedef struct {
    token_kind_t token_kind;
    pcre2_code *pattern;
} entry_t;

typedef struct {
    token_kind_t kind;
    const char *pattern;
} raw_entry_t;

static raw_entry_t g_raw_spec[] = {
    { .kind = TOKEN_KIND_INTERNAL_NONE, .pattern = "^\\s+" }, // Whitespace
    { .kind = TOKEN_KIND_INTERNAL_NONE, .pattern = "^//.*" }, // Single line comment
    { .kind = TOKEN_KIND_INTERNAL_NONE, .pattern = "^/\\*[\\s\\S]*?\\*/" }, // Multi line comment
    #define TOKEN(ID, _, PATTERN) { .kind = TOKEN_KIND_##ID, .pattern = PATTERN },
    #include "tokens.def"
    #undef TOKEN
};

static size_t g_spec_size = 0;
static entry_t *g_spec_entries = NULL;

void spec_ensure() {
    if(g_spec_entries != NULL) return;
    size_t size = sizeof(g_raw_spec) / sizeof(raw_entry_t);
    entry_t *entries = malloc(sizeof(entry_t) * size);

    for(size_t i = 0; i < size; i++) {
        int error_code;
        PCRE2_SIZE error_offset;
        pcre2_code *code = pcre2_compile((const uint8_t *) g_raw_spec[i].pattern, PCRE2_ZERO_TERMINATED, 0, &error_code, &error_offset, NULL);
        if(code == NULL) {
            char error_message[120];
            pcre2_get_error_message(error_code, (uint8_t *) error_message, 120);
            log_fatal("failed compiling pattern '%s' (%s)", g_raw_spec[i].pattern, error_message);
        }
        entries[i] = (entry_t) { .token_kind = g_raw_spec[i].kind, .pattern = code };
    }

    g_spec_size = size;
    g_spec_entries = entries;
}

spec_match_t spec_match(const char *string, size_t string_length) {
    spec_ensure();
    pcre2_match_data *md = pcre2_match_data_create(1, NULL);
    for(size_t i = 0; i < g_spec_size; i++) {
        int match_count = pcre2_match(g_spec_entries[i].pattern, (const uint8_t *) string, string_length, 0, 0, md, NULL);
        if(match_count <= 0) continue;

        PCRE2_SIZE size;
        int error_code = pcre2_substring_length_bynumber(md, 0, &size);
        if(error_code != 0) {
            char error_message[120];
            pcre2_get_error_message(error_code, (uint8_t *) error_message, 120);
            log_fatal("error during spec matching '%s' (%s)", g_raw_spec[i].pattern, error_message);
        }
        pcre2_match_data_free(md);
        return (spec_match_t) { .kind = g_spec_entries[i].token_kind, .size = size };
    }
    pcre2_match_data_free(md);
    return (spec_match_t) { .kind = TOKEN_KIND_INTERNAL_NONE, .size = 0 };
}