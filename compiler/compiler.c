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

static void print_tree(charon_memory_allocator_t *allocator, charon_element_t *element, int depth) {
    for(size_t i = 0; i < depth * 4; i++) printf(" ");

    switch(charon_element_type(element->inner)) {
        case CHARON_ELEMENT_TYPE_TRIVIA: assert(false);
        case CHARON_ELEMENT_TYPE_NODE:   {
            charon_node_kind_t node_kind = charon_element_node_kind(element->inner);
            printf("%s%s%s\n", node_kind == CHARON_NODE_KIND_ERROR ? "\e[41m" : "", charon_node_kind_tostring(node_kind), "\e[0m");

            size_t child_count = charon_element_node_child_count(element->inner);
            for(size_t i = 0; i < child_count; i++) {
                print_tree(allocator, charon_element_wrap_node_child(allocator, element, i), depth + 1);
            }
            break;
        }
        case CHARON_ELEMENT_TYPE_TOKEN:
            charon_token_kind_t token_kind = charon_element_token_kind(element->inner);
            const char *kind_text = charon_token_kind_tostring(token_kind);
            const char *text = charon_element_token_text(element->inner);
            printf("Token(");
            if(text == nullptr || strcmp(kind_text, text) != 0) {
                printf("%s", kind_text);
                if(text != nullptr) printf(" ");
            }
            if(text != nullptr) printf("`%s`", text);
            printf(")\n");
            break;
    }
}

int main(int argc, char **argv) {
    if(argc < 2) {
        printf("no path provided\n");
        exit(EXIT_FAILURE);
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

    size_t text_length = st.st_size;
    char *text = malloc(text_length);
    if(fread(text, 1, text_length, file) != text_length) {
        printf("read source file '%s' (%s)\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(fclose(file) != 0) printf("close source file '%s' (%s)\n", name, strerror(errno));

    // Setup project context
    charon_memory_allocator_t *allocator = charon_memory_allocator_make();
    charon_element_cache_t *cache = charon_element_cache_make(allocator);

    // Parse
    charon_lexer_t *lexer = charon_lexer_make(cache, text, text_length);
    charon_parser_t *parser = charon_parser_make(cache, lexer);

    const charon_element_inner_t *root_element = charon_parser_parse_root(parser);

    print_tree(allocator, charon_element_wrap_root(allocator, root_element), 0);

    charon_lexer_destroy(lexer);
    charon_parser_destroy(parser);

    free(text);

    // Cleanup project context
    charon_element_cache_destroy(cache);
    charon_memory_allocator_free(allocator);

    return 0;
}
