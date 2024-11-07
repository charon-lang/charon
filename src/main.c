#include "lib/source.h"
#include "lib/log.h"
#include "lexer/tokenizer.h"
#include "parser/parser.h"
#include "semantics/semantics.h"
#include "codegen/codegen.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <libgen.h>
#include <sys/stat.h>
#include <errno.h>
#include <llvm-c/Core.h>

#define SEMVER_PRE_RELEASE "alpha.4"
#define SEMVER_MAJOR 1
#define SEMVER_MINOR 0
#define SEMVER_PATCH 0

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

static char *strdup_basename(char *path) {
    char *original = strdup(path);
    char *temporary = basename(original);
    char *new = strdup(temporary);
    free(original);
    return new;
}

int main(int argc, char *argv[]) {
    enum { DEFAULT, COMPILE, VERSION } mode = DEFAULT;
    enum { OBJECT, IR } format = OBJECT;
    enum { NONE, O1, O2, O3 } optimization = O1;

    while(true) {
        int option_index = 0;
        int option = getopt_long(argc, argv, "ci", (struct option[]) {
            { .name = "compile", .has_arg = no_argument, .val = 'c' },
            { .name = "version", .has_arg = no_argument, .val = 'v' },
            { .name = "llvm-ir", .has_arg = no_argument, .val = IR, .flag = (int *) &format },

            { .name = "O0", .has_arg = no_argument, .val = NONE, .flag = (int *) &optimization },
            { .name = "O1", .has_arg = no_argument, .val = O1, .flag = (int *) &optimization },
            { .name = "O2", .has_arg = no_argument, .val = O2, .flag = (int *) &optimization },
            { .name = "O3", .has_arg = no_argument, .val = O3, .flag = (int *) &optimization },
            {}
        }, &option_index);
        if(option == -1) break;

        switch(option) {
            case 'c': mode = COMPILE; break;
            case 'v': mode = VERSION; break;
        }
    }

    const char *optstr = NULL;
    switch(optimization) {
        case NONE: break;
        case O1: optstr = "default<O1>"; break;
        case O2: optstr = "default<O2>"; break;
        case O3: optstr = "default<O3>"; break;
    }

    switch(mode) {
        case VERSION:
            printf("Charon %u.%u.%u%s%s\n", SEMVER_MAJOR, SEMVER_MINOR, SEMVER_PATCH, SEMVER_PRE_RELEASE == NULL ? "" : "-", SEMVER_PRE_RELEASE);
            printf("Built at " __DATE__ " " __TIME__ "\n");
            if(LLVMIsMultithreaded()) printf("Multithreaded\n");
            break;
        case DEFAULT:
        case COMPILE:
            for(int i = optind; i < argc; i++) {
                char *name = strdup_basename(argv[i]);

                FILE *file = fopen(argv[i], "r");
                if(file == NULL) log_fatal("open source file '%s' (%s)", name, strerror(errno));

                struct stat st;
                if(fstat(fileno(file), &st) != 0) log_fatal("stat source file '%s' (%s)", name, strerror(errno));

                void *data = malloc(st.st_size);
                if(fread(data, 1, st.st_size, file) != st.st_size) log_fatal("read source file '%s' (%s)", name, strerror(errno));
                if(fclose(file) != 0) log_warning("close source file '%s' (%s)", name, strerror(errno));

                source_t *source = source_make(name, data, st.st_size);

                tokenizer_t *tokenizer = tokenizer_make(source);
                hlir_node_t *root_node = parser_root(tokenizer);
                tokenizer_free(tokenizer);

                llir_namespace_t *root_namespace = llir_namespace_make(NULL);
                llir_node_t *llir_root_node = semantics(root_node, root_namespace);

                char *extension = find_extension(argv[i], '.', '/');
                if(extension == NULL || strcmp(extension, ".charon") != 0) {
                    log_warning("skipping source file `%s` due to an unknown extension `%s`", argv[i], extension);
                    continue;
                }
                char *extensionless_path = strip_extension(argv[i], '.', '/');

                switch(format) {
                    case OBJECT: codegen(llir_root_node, root_namespace, add_extension(extensionless_path, "o", '.'), optstr); break;
                    case IR: codegen_ir(llir_root_node, root_namespace, add_extension(extensionless_path, "ll", '.')); break;
                }
                free(extensionless_path);

                source_free(source);
            }
            break;
    }
    return EXIT_SUCCESS;
}
