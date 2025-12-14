#include "charon/diag.h"
#include "charon/path.h"
#include "charon/utf8.h"

#include <assert.h>
#include <charon/element.h>
#include <charon/lexer.h>
#include <charon/memory.h>
#include <charon/parser.h>
#include <charon/util.h>
#include <errno.h>
#include <libgen.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool raw_output = true;
bool quiet_mode = false;

#define PRINTF_IF_NOT_QUIET(...)                 \
    do {                                         \
        if(!quiet_mode) { printf(__VA_ARGS__); } \
    } while(0)

char *ansi_color(const char *text) {
    if(!raw_output) return "";

    return strdup(text);
}

static void print_tree(charon_memory_allocator_t *allocator, charon_element_t *element, int depth) {
    for(size_t i = 0; i < depth * 4; i++) PRINTF_IF_NOT_QUIET(" ");

    switch(charon_element_type(element->inner)) {
        case CHARON_ELEMENT_TYPE_TRIVIA: assert(false);
        case CHARON_ELEMENT_TYPE_NODE:   {
            charon_node_kind_t node_kind = charon_element_node_kind(element->inner);

            PRINTF_IF_NOT_QUIET("%s%s%s\n", node_kind == CHARON_NODE_KIND_ERROR ? ansi_color("\e[41m") : "", charon_node_kind_tostring(node_kind), ansi_color("\e[0m"));

            size_t child_count = charon_element_node_child_count(element->inner);
            for(size_t i = 0; i < child_count; i++) { print_tree(allocator, charon_element_wrap_node_child(allocator, element, i), depth + 1); }
            break;
        }
        case CHARON_ELEMENT_TYPE_TOKEN:
            charon_token_kind_t token_kind = charon_element_token_kind(element->inner);
            const char *kind_text = charon_token_kind_tostring(token_kind);
            const charon_utf8_text_t *token_text = charon_element_token_text(element->inner);
            const char *str = token_text == nullptr ? nullptr : charon_utf8_as_string(token_text);
            PRINTF_IF_NOT_QUIET("Token(");
            if(str == nullptr || strcmp(kind_text, str) != 0) {
                PRINTF_IF_NOT_QUIET("%s", kind_text);
                if(str != nullptr) PRINTF_IF_NOT_QUIET(" ");
            }
            if(str != nullptr) PRINTF_IF_NOT_QUIET("`%s`", str);
            PRINTF_IF_NOT_QUIET(")\n");
            break;
    }
}

int main(int argc, char **argv) {
    if(argc < 2) {
        printf("no path provided\n");
        exit(EXIT_FAILURE);
    }

    if(argc == 3) {
        // @note: very technical option names
        // ignores ANSI support
        if(strcmp(argv[2], "--raw-output") == 0) {
            raw_output = false;
        }
        // disables ALL printing
        else if(strcmp(argv[2], "--quiet") == 0)
        {
            quiet_mode = true;
        } else {
            printf("unknown option: %s\n", argv[2]);
            exit(EXIT_FAILURE);
        }
    }

    char *name = basename(strdup(argv[1]));

    FILE *file = fopen(argv[1], "r");
    if(file == NULL) {
        printf("open source file '%s' (%s)\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }

    struct stat st;
    if(fstat(fileno(file), &st) != 0) {
        printf("stat source file '%s' (%s)\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }

    size_t data_size = st.st_size;
    void *data = malloc(data_size);
    if(fread(data, 1, data_size, file) != data_size) {
        printf("read source file '%s' (%s)\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(fclose(file) != 0) printf("close source file '%s' (%s)\n", name, strerror(errno));

    // Setup project context
    charon_memory_allocator_t *allocator = charon_memory_allocator_make();
    charon_element_cache_t *cache = charon_element_cache_make(allocator);

    // Parse
    charon_utf8_text_t *text = charon_utf8_from(data, data_size);
    charon_lexer_t *lexer = charon_lexer_make(cache, text);
    charon_parser_t *parser = charon_parser_make(cache, lexer);

    charon_parser_output_t parser_output = charon_parser_parse_root(parser);

    charon_element_t *root_element = charon_element_wrap_root(allocator, parser_output.root);
    print_tree(allocator, root_element, 0);

    charon_diag_item_t *next_diag = parser_output.diagnostics;
    while(next_diag != nullptr) {
        charon_diag_item_t *diag = next_diag;
        next_diag = diag->next;

        charon_element_t *current_element = root_element;
        for(size_t i = 0; i < diag->path->length; i++) {
            size_t child_index = diag->path->steps[i];
            assert(charon_element_type(current_element->inner) == CHARON_ELEMENT_TYPE_NODE);
            assert(charon_element_node_child_count(current_element->inner) > child_index);

            current_element = charon_element_wrap_node_child(allocator, current_element, child_index);
        }

        PRINTF_IF_NOT_QUIET("DIAGNOSTIC %s %s\n", charon_diag_tostring(diag->kind), charon_diag_fmt(diag->kind, diag->data));
        print_tree(allocator, current_element, 0);

        charon_path_destroy(diag->path);
        free(diag);
    }

    charon_lexer_destroy(lexer);
    charon_parser_destroy(parser);

    free(text);
    free(data);

    // Cleanup project context
    charon_element_cache_destroy(cache);
    charon_memory_allocator_free(allocator);

    return 0;
}
