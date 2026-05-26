#include "charon/lexer.h"
#include "charon/syntax/token.h"
#include "charon/syntax/trivia.h"

#include <errno.h>
#include <libgen.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern void charon_test(const void *data, size_t data_size);

int main(int argc, const char **argv) {
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

    size_t data_size = st.st_size;
    void *data = malloc(data_size);
    if(fread(data, 1, data_size, file) != data_size) {
        printf("read source file '%s' (%s)\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(fclose(file) != 0) printf("close source file '%s' (%s)\n", name, strerror(errno));

    charon_lexer_t *lexer = charon_lexer_new(data, data_size);

    while(true) {
        charon_lexer_token_t token = charon_lexer_advance(lexer);
        if(!token.is_trivia && token.kind.token == CHARON_TOKEN_KIND_EOF) break;

        bool print_data = true;
        size_t token_length = token.end - token.start;

        printf("%lu - %lu: ", token.start, token.end);
        if(token.is_trivia) {
            printf("%s ", charon_trivia_kind_tostring(token.kind.trivia));
            if(token.kind.trivia == CHARON_TRIVIA_KIND_NEWLINE) {
                print_data = false;
                printf("\n");
            }
        } else {
            printf("%s ", charon_token_kind_tostring(token.kind.token));
            if(token.kind.token == CHARON_TOKEN_KIND_UNKNOWN) {
                print_data = false;
                printf("(");
                for(size_t i = 0; i < token_length; i++) { printf("%#x ", *(uint8_t *) (data + token.start + i)); }
                printf(")\n");
            }
        }

        if(print_data) { printf("[%.*s]\n", (int) token_length, &((char *) data)[token.start]); }
    }

    charon_lexer_destroy(lexer);

    charon_test(data, data_size);

    return 0;
}
