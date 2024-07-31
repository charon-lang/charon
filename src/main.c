#include "lib/source.h"
#include "lib/log.h"
#include "lexer/tokenizer.h"
#include "codegen/codegen.h"
#include "ir/node.h"
#include "parser/parser.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <llvm-c/Core.h>

static char *find_extension(char *path, char extension_separator, char path_separator) {
    if(path == NULL) return NULL;

    char *last_extension = strrchr(path, extension_separator);
    char *last_path = (path_separator == '\0') ? NULL : strrchr(path, path_separator);
    if(last_extension == NULL || last_path >= last_extension) return NULL;

    return last_extension;
}

static char *strip_extension(char *path, char extension_separator, char path_separator) {
    if(path == NULL) return NULL;

    char *extension = find_extension(path, extension_separator, path_separator);
    if(extension == NULL) return strdup(path);

    size_t new_length = (uintptr_t) extension - (uintptr_t) path;
    char *new = malloc(new_length + 1);
    memcpy(new, path, new_length);
    new[new_length] = '\0';
    return new;
}

static char *add_extension(char *path, char *extension, char extension_separator) {
    if(path == NULL) return NULL;

    size_t path_length = strlen(path);
    size_t extension_length = strlen(extension);
    char *new = malloc(path_length + 1 + extension_length + 1);
    memcpy(new, path, path_length);
    new[path_length] = extension_separator;
    memcpy(&new[path_length + 1], extension, extension_length);
    new[path_length + 1 + extension_length] = '\0';
    return new;
}

int main(int argc, char *argv[]) {
    enum { DEFAULT, COMPILE, INFO } mode = DEFAULT;
    enum { OBJECT, IR } format = OBJECT;

    while(true) {
        int option_index = 0;
        int option = getopt_long(argc, argv, "ci", (struct option[]) {
            { .name = "compile", .has_arg = no_argument, .val = 'c' },
            { .name = "info", .has_arg = no_argument, .val = 'i' },
            { .name = "llvm-ir", .has_arg = no_argument, .val = IR, .flag = &format },
            {}
        }, &option_index);
        if(option == -1) break;

        switch(option) {
            case 'c': mode = COMPILE; break;
            case 'i': mode = INFO; break;
        }
    }

    switch(mode) {
        case INFO:
            printf("Charon (dev)\n");
            if(LLVMIsMultithreaded()) printf("Multithreaded\n");
            break;
        case DEFAULT:
        case COMPILE:
            size_t file_count = 0;
            char **files = NULL;
            while(optind < argc) {
                files = realloc(files, ++file_count * sizeof(char *));
                files[file_count - 1] = argv[optind];
            }

            for(int i = 1; i < argc; i++) {
                source_t *source = source_from_path(argv[i]);

                tokenizer_t *tokenizer = tokenizer_make(source);
                ir_node_t *node = parser_root(tokenizer);
                tokenizer_free(tokenizer);

                char *extension = find_extension(argv[i], '.', '/');
                if(extension == NULL || strcmp(extension, ".charon") != 0) {
                    log_warning("skipping source file `%s` due to an unknown extension `%s`", argv[i], extension);
                    continue;
                }
                char *extensionless_path = strip_extension(argv[i], '.', '/');
                char *path = add_extension(extensionless_path, "o", '.');
                free(extensionless_path);

                switch(format) {
                    case OBJECT: codegen(node, path, ""); break;
                    case IR: codegen_ir(node, path); break;
                }

                source_free(source);
            }
            break;
    }
    return EXIT_SUCCESS;
}
