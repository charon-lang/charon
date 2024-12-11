#include "lib/source.h"
#include "lib/log.h"
#include "lib/alloc.h"
#include "lexer/tokenizer.h"
#include "parser/parser.h"
#include "lower/lower.h"
#include "passes/pass.h"
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

#define SEMVER_PRE_RELEASE "alpha.4.1"
#define SEMVER_MAJOR 1
#define SEMVER_MINOR 0
#define SEMVER_PATCH 0

#define VALID_MEMORY_MODELS "default, kernel, tiny, small, medium, large"

#define BASH_COLOR(COLOR, TEXT) "\e[" COLOR "m" TEXT "\e[0m"

// static char *find_extension(char *path, char extension_separator, char path_separator) {
//     if(path == NULL) return NULL;

//     char *last_extension = strrchr(path, extension_separator);
//     char *last_path = (path_separator == '\0') ? NULL : strrchr(path, path_separator);
//     if(last_extension == NULL || last_path >= last_extension) return NULL;

//     return last_extension;
// }

// static char *strip_extension(char *path, char extension_separator, char path_separator) {
//     if(path == NULL) return NULL;

//     char *extension = find_extension(path, extension_separator, path_separator);
//     if(extension == NULL) return strdup(path);

//     size_t new_length = (uintptr_t) extension - (uintptr_t) path;
//     char *new = malloc(new_length + 1);
//     memcpy(new, path, new_length);
//     new[new_length] = '\0';
//     return new;
// }

static char *add_extension(const char *path, char *extension, char extension_separator) {
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

static void compile(source_t **sources, size_t source_count, const char *dest_path, bool ir, LLVMCodeModel code_model, const char *optstr);

int main(int argc, char *argv[]) {
    enum { DEFAULT, COMPILE, HELP } mode = DEFAULT;
    enum { OBJECT, IR } format = OBJECT;
    enum { NONE, O1, O2, O3 } optimization = O1;

    LLVMCodeModel code_model = LLVMCodeModelDefault;

    const char *dest_path = NULL;

    while(true) {
        int option_index = 0;
        int option = getopt_long(argc, argv, "ci", (struct option[]) {
            { .name = "help", .has_arg = no_argument, .val = 'h' },
            { .name = "compile", .has_arg = no_argument, .val = 'c' },

            { .name = "o", .has_arg = required_argument, .val = 'o' },

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
            case 'o':
                dest_path = strdup(optarg);
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
            for(int i = optind, j = 0; i < argc; i++, j++) {
                char *name = strdup_basename(argv[i]);

                FILE *file = fopen(argv[i], "r");
                if(file == NULL) log_fatal("open source file '%s' (%s)", name, strerror(errno));

                struct stat st;
                if(fstat(fileno(file), &st) != 0) log_fatal("stat source file '%s' (%s)", name, strerror(errno));

                void *data = malloc(st.st_size);
                if(fread(data, 1, st.st_size, file) != st.st_size) log_fatal("read source file '%s' (%s)", name, strerror(errno));
                if(fclose(file) != 0) log_warning("close source file '%s' (%s)", name, strerror(errno));

                sources[j] = source_make(name, data, st.st_size);
            }

            if(count == 0) {
                log_warning("No sources provided\n");
                break;
            }

            if(dest_path == NULL) {
                if(count > 0) {
                    dest_path = "out";
                } else {
                    dest_path = sources[0]->name;
                }
                dest_path = add_extension(dest_path, format == IR ? "ll" : "o", '.');
            }

            compile(sources, count, dest_path, format == IR, code_model, optstr);

            for(size_t i = 0; i < count; i++) source_free(sources[i]);
            free((char *) dest_path);
            free(sources);
            break;
    }
    return EXIT_SUCCESS;
}

static void compile(source_t **sources, size_t source_count, const char *dest_path, bool ir, LLVMCodeModel code_model, const char *optstr) {
    hlir_node_t *hlir_root_nodes[source_count];

    /* Parse: Source -> Tokens -> AST */
    memory_allocator_t *hlir_allocator = memory_allocator_make();
    memory_active_allocator_set(hlir_allocator);
    for(size_t i = 0; i < source_count; i++) {
        tokenizer_t *tokenizer = tokenizer_make(sources[i]);
        hlir_root_nodes[i] = parser_root(tokenizer);
        tokenizer_free(tokenizer);
    }
    memory_active_allocator_set(NULL);

    /* Lower: AST -> IR */
    memory_allocator_t *ir_allocator = memory_allocator_make();
    memory_allocator_t *work_allocator = memory_allocator_make();
    memory_active_allocator_set(ir_allocator);

    ir_unit_t unit = { .root_namespace = IR_NAMESPACE_INIT };
    ir_type_cache_t *type_cache = ir_type_cache_make();
    lower_context_t *lower_context = lower_context_make(work_allocator, &unit, type_cache);

    for(size_t i = 0; i < source_count; i++) lower_populate_namespace(lower_context, hlir_root_nodes[i]);
    for(size_t i = 0; i < source_count; i++) lower_nodes(lower_context, hlir_root_nodes[i]);
    memory_allocator_free(work_allocator);
    memory_allocator_free(hlir_allocator);

    /* Passes: IR -> IR */
    pass_eval_types(&unit, type_cache);

    /* Codegen: IR -> Binary */
    if(ir) {
        codegen_ir(&unit, type_cache, dest_path);
    } else {
        codegen(&unit, type_cache, dest_path, optstr, code_model);
    }

    memory_active_allocator_set(NULL);
    memory_allocator_free(ir_allocator);
}