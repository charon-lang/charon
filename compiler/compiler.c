#include "charon/lexer.h"
#include "charon/source.h"
#include "charon/token.h"

#include <errno.h>
#include <libgen.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

    void *data = malloc(st.st_size);
    if(fread(data, 1, st.st_size, file) != st.st_size) {
        printf("read source file '%s' (%s)\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(fclose(file) != 0) printf("close source file '%s' (%s)\n", name, strerror(errno));

    // Setup project context
    charon_token_cache_t *token_cache = charon_token_cache_make();

    // Setup unit context
    charon_source_t *source = charon_source_make("test", data, st.st_size);

    free(data);

    // Lex Unit
    charon_lexer_t *lexer = charon_lexer_make(token_cache, source);

    while(!charon_lexer_is_eof(lexer)) {
        charon_token_t *token = charon_lexer_advance(lexer);
        printf("%s\n", charon_token_kind_tostring(token->kind));
    }

    charon_lexer_destroy(lexer);

    // Cleanup unit context
    charon_source_destroy(source);

    // Cleanup project context
    charon_token_cache_destroy(token_cache);
    return 0;
}
