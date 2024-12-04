#include "lib/source.h"
#include "lib/log.h"
#include "lib/alloc.h"
#include "lexer/tokenizer.h"
#include "parser/parser.h"
#include "lower/lower.h"
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
#include <llvm-c/TargetMachine.h>

#define SEMVER_PRE_RELEASE "alpha.4"
#define SEMVER_MAJOR 1
#define SEMVER_MINOR 0
#define SEMVER_PATCH 0

#define VALID_MEMORY_MODELS "default, kernel, tiny, small, medium, large"

#define BASH_COLOR(COLOR, TEXT) "\e[" COLOR "m" TEXT "\e[0m"

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

static void compile(source_t **sources, size_t source_count, const char **dest_path, bool ir, LLVMCodeModel code_model, const char *optstr);

int main(int argc, char *argv[]) {
    enum { DEFAULT, COMPILE, HELP } mode = DEFAULT;
    enum { OBJECT, IR } format = OBJECT;
    enum { NONE, O1, O2, O3 } optimization = O1;

    LLVMCodeModel code_model = LLVMCodeModelDefault;

    while(true) {
        int option_index = 0;
        int option = getopt_long(argc, argv, "ci", (struct option[]) {
            { .name = "help", .has_arg = no_argument, .val = 'h' },
            { .name = "compile", .has_arg = no_argument, .val = 'c' },

            { .name = "llvm-ir", .has_arg = no_argument, .val = IR, .flag = (int *) &format },
            { .name = "memory-model", .has_arg = required_argument, .val = 'm' },

            { .name = "O0", .has_arg = no_argument, .val = NONE, .flag = (int *) &optimization },
            { .name = "O1", .has_arg = no_argument, .val = O1, .flag = (int *) &optimization },
            { .name = "O2", .has_arg = no_argument, .val = O2, .flag = (int *) &optimization },
            { .name = "O3", .has_arg = no_argument, .val = O3, .flag = (int *) &optimization },
            {}
        }, &option_index);
        if(option == -1) break;

        switch(option) {
            case 'h': mode = HELP; break;
            case 'c': mode = COMPILE; break;
            case 'm':
                if(strcmp(optarg, "default") == 0) code_model = LLVMCodeModelDefault;
                else if(strcmp(optarg, "kernel") == 0) code_model = LLVMCodeModelKernel;
                else if(strcmp(optarg, "tiny") == 0) code_model = LLVMCodeModelTiny;
                else if(strcmp(optarg, "small") == 0) code_model = LLVMCodeModelSmall;
                else if(strcmp(optarg, "medium") == 0) code_model = LLVMCodeModelMedium;
                else if(strcmp(optarg, "large") == 0) code_model = LLVMCodeModelLarge;
                else log_fatal("unknown memory model `%s` (Use one of: " VALID_MEMORY_MODELS ")", optarg);
                break;
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
        case HELP: {
            printf("Charon %u.%u.%u%s%s%s\n", SEMVER_MAJOR, SEMVER_MINOR, SEMVER_PATCH, SEMVER_PRE_RELEASE == NULL ? "" : "-", SEMVER_PRE_RELEASE, LLVMIsMultithreaded() ? " (multithreaded)" : "");
            printf("Built at " __DATE__ " " __TIME__ "\n");
            printf(
                "Usage: charon "
                "[" BASH_COLOR("1", "--help") "|" BASH_COLOR("1", "--compile") "] "
                "[" BASH_COLOR("1", "-O") BASH_COLOR("4", "level") "] "
                "[" BASH_COLOR("1", "--llvm-ir") "] "
                "[" BASH_COLOR("1", "--memory-model") "=" BASH_COLOR("4", "model") "] "
                BASH_COLOR("4", "files") "...\n"
            );
            printf("\n");
            printf(
                "Options:\n"
                "  " BASH_COLOR("1", "--help") ", " BASH_COLOR("1", "--compile") "\n"
                "      Select operation, " BASH_COLOR("4", "compile") " is the default mode.\n"
                "  " BASH_COLOR("1", "--llvm-ir") "\n"
                "      Compile to LLVM IR.\n"
                "  " BASH_COLOR("1", "--memory-model") "=" BASH_COLOR("4", "model") "\n"
                "      Code model to use. Choose one of: " VALID_MEMORY_MODELS ".\n"
                "  " BASH_COLOR("1", "-O") BASH_COLOR("4", "level") "\n"
                "      Optimization level. Valid levels are: 0, 1, 2, 3.\n"
            );
        } break;
        case DEFAULT:
        case COMPILE:
            size_t count = argc - optind;
            source_t **sources = reallocarray(NULL, count, sizeof(source_t *));
            char **dest_paths = reallocarray(NULL, count, sizeof(char *));
            for(int i = optind, j = 0; i < argc; i++, j++) {
                char *name = strdup_basename(argv[i]);

                FILE *file = fopen(argv[i], "r");
                if(file == NULL) log_fatal("open source file '%s' (%s)", name, strerror(errno));

                struct stat st;
                if(fstat(fileno(file), &st) != 0) log_fatal("stat source file '%s' (%s)", name, strerror(errno));

                void *data = malloc(st.st_size);
                if(fread(data, 1, st.st_size, file) != st.st_size) log_fatal("read source file '%s' (%s)", name, strerror(errno));
                if(fclose(file) != 0) log_warning("close source file '%s' (%s)", name, strerror(errno));

                char *extension = find_extension(argv[i], '.', '/');
                if(extension == NULL || strcmp(extension, ".charon") != 0) {
                    log_warning("skipping source file `%s` due to an unknown extension `%s`", argv[i], extension);
                    continue;
                }
                char *extensionless_path = strip_extension(argv[i], '.', '/');
                char *dest_path = add_extension(extensionless_path, format == IR ? "ll" : "o", '.');
                free(extensionless_path);

                sources[j] = source_make(name, data, st.st_size);
                dest_paths[j] = dest_path;
            }

            compile(sources, count, (const char **) dest_paths, format == IR, code_model, optstr);

            for(size_t i = 0; i < count; i++) {
                source_free(sources[i]);
                free(dest_paths[i]);
            }
            free(dest_paths);
            free(sources);
            break;
    }
    return EXIT_SUCCESS;
}

static void compile(source_t **sources, size_t source_count, const char **dest_paths, bool ir, LLVMCodeModel code_model, const char *optstr) {
    struct {
        memory_allocator_t *allocator;
        hlir_node_t *root_node;
    } hlir_data[source_count];

    /* Parse: Source -> Tokens -> LLIR */
    for(size_t i = 0; i < source_count; i++) {
        memory_allocator_t *hlir_allocator = memory_allocator_make();

        tokenizer_t *tokenizer = tokenizer_make(sources[i]);
        memory_active_allocator_set(hlir_allocator);
        hlir_node_t *root_node = parser_root(tokenizer);
        memory_active_allocator_set(NULL);
        tokenizer_free(tokenizer);

        hlir_data[i].allocator = hlir_allocator;
        hlir_data[i].root_node = root_node;
    }

    /* Lower: LLIR -> HLIR */
    memory_allocator_t *llir_allocator = memory_allocator_make();
    memory_allocator_t *work_allocator = memory_allocator_make();
    memory_active_allocator_set(llir_allocator);

    llir_namespace_t *root_namespace = llir_namespace_make(NULL);
    llir_type_cache_t *anon_type_cache = llir_type_cache_make();

    for(size_t i = 0; i < source_count; i++) lower_populate_namespace(hlir_data[i].root_node, root_namespace, anon_type_cache);

    llir_node_t *llir_root_node[source_count];
    for(size_t i = 0; i < source_count; i++) llir_root_node[i] = lower_nodes(hlir_data[i].root_node, root_namespace, anon_type_cache);

    for(size_t i = 0; i < source_count; i++) memory_allocator_free(hlir_data[i].allocator);
    memory_allocator_free(work_allocator);

    /* Codegen: LLIR -> Binary */
    for(size_t i = 0; i < source_count; i++) {
        if(ir) {
            codegen_ir(llir_root_node[i], root_namespace, anon_type_cache, dest_paths[i]);
        } else {
            codegen(llir_root_node[i], root_namespace, anon_type_cache, dest_paths[i], optstr, code_model);
        }
    }

    memory_active_allocator_set(NULL);
    memory_allocator_free(llir_allocator);
}