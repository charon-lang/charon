#include "ast/node.h"
#include "codegen/codegen.h"
#include "lexer/tokenizer.h"
#include "lib/log.h"
#include "lib/memory.h"
#include "lib/source.h"
#include "parser/parser.h"
#include "pass/pass.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define SEMVER_PRE_RELEASE "alpha.4.1"
#define SEMVER_MAJOR 1
#define SEMVER_MINOR 0
#define SEMVER_PATCH 0

#define C(COLOR, TEXT) "\e[" COLOR "m" TEXT "\e[0m"

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

static char *strdup_basename(const char *path) {
    char *original = strdup(path);
    char *temporary = basename(original);
    char *new = strdup(temporary);
    free(original);
    return new;
}

int main(int argc, const char *argv[]) {
    enum {
        COMPILE,
        RUN
    } mode;

    if(argc < 2) log_fatal("missing subcommand");

    if(strcasecmp(argv[1], "info") == 0) {
        printf("Charon %u.%u.%u%s\n", SEMVER_MAJOR, SEMVER_MINOR, SEMVER_PATCH, SEMVER_PRE_RELEASE == NULL ? "" : "-" SEMVER_PRE_RELEASE);
        printf("Built at " __DATE__ " " __TIME__ "\n");
        return EXIT_SUCCESS;
    } else if(strcasecmp(argv[1], "compile") == 0)
        mode = COMPILE;
    else if(strcasecmp(argv[1], "run") == 0)
        mode = RUN;
    else
        log_fatal("invalid subcommand");

    size_t source_count = 0;
    source_t **sources = NULL;

    const char *dest_path = NULL;
    bool emit_ir = false;
    codegen_relocation_t reloc_mode = CODEGEN_RELOCATION_DEFAULT;
    codegen_code_model_t code_model = CODEGEN_CODE_MODEL_DEFAULT;
    codegen_optimization_t optimization = mode == CODEGEN_OPTIMIZATION_O1;
    const char *features = "";
    for(int i = 2; i < argc; i++) {
        if(argv[i][0] != '-') {
            char *name = strdup_basename(argv[i]);

            FILE *file = fopen(argv[i], "r");
            if(file == NULL) log_fatal("open source file '%s' (%s)", name, strerror(errno));

            struct stat st;
            if(fstat(fileno(file), &st) != 0) log_fatal("stat source file '%s' (%s)", name, strerror(errno));

            void *data = malloc(st.st_size);
            if(fread(data, 1, st.st_size, file) != st.st_size) log_fatal("read source file '%s' (%s)", name, strerror(errno));
            if(fclose(file) != 0) log_warning("close source file '%s' (%s)", name, strerror(errno));

            sources = reallocarray(sources, ++source_count, sizeof(source_t *));
            sources[source_count - 1] = source_make(name, data, st.st_size);
            continue;
        }

        if(strcmp(argv[i], "-O0") == 0) {
            optimization = CODEGEN_OPTIMIZATION_NONE;
            continue;
        }
        if(strcmp(argv[i], "-O1") == 0) {
            optimization = CODEGEN_OPTIMIZATION_O1;
            continue;
        }
        if(strcmp(argv[i], "-O2") == 0) {
            optimization = CODEGEN_OPTIMIZATION_O2;
            continue;
        }
        if(strcmp(argv[i], "-O3") == 0) {
            optimization = CODEGEN_OPTIMIZATION_O3;
            continue;
        }

        if(strncasecmp(argv[i], "--optimization=", 15) == 0) {
            char c = argv[i][15];
            switch(c) {
                case '0': optimization = CODEGEN_OPTIMIZATION_NONE; break;
                case '1': optimization = CODEGEN_OPTIMIZATION_O1; break;
                case '2': optimization = CODEGEN_OPTIMIZATION_O2; break;
                case '3': optimization = CODEGEN_OPTIMIZATION_O3; break;
                default:  goto invalid_optimization;
            }
            if(argv[i][16] != '\0') {
            invalid_optimization:
                log_warning("invalid optimization level '%s'", &argv[i][15]);
            }
            continue;
        }

        if(strncasecmp(argv[i], "--features=", 11) == 0) {
            features = &argv[i][11];
            continue;
        }

        if(mode == COMPILE && strncasecmp(argv[i], "--code-model=", 13) == 0) {
            const char *left = &argv[i][13];
            if(strcasecmp(left, "default") == 0)
                code_model = CODEGEN_CODE_MODEL_DEFAULT;
            else if(strcasecmp(left, "kernel") == 0)
                code_model = CODEGEN_CODE_MODEL_KERNEL;
            else if(strcasecmp(left, "tiny") == 0)
                code_model = CODEGEN_CODE_MODEL_TINY;
            else if(strcasecmp(left, "small") == 0)
                code_model = CODEGEN_CODE_MODEL_SMALL;
            else if(strcasecmp(left, "medium") == 0)
                code_model = CODEGEN_CODE_MODEL_MEDIUM;
            else if(strcasecmp(left, "large") == 0)
                code_model = CODEGEN_CODE_MODEL_LARGE;
            else
                log_warning("invalid code model '%s', ignoring", left);
            continue;
        }

        if(mode == COMPILE && strncasecmp(argv[i], "--reloc=", 8) == 0) {
            const char *left = &argv[i][8];
            if(strcasecmp(left, "default") == 0)
                reloc_mode = CODEGEN_RELOCATION_DEFAULT;
            else if(strcasecmp(left, "static") == 0)
                reloc_mode = CODEGEN_RELOCATION_STATIC;
            else if(strcasecmp(left, "pic") == 0)
                reloc_mode = CODEGEN_RELOCATION_PIC;
            else
                log_warning("invalid relocation mode '%s', ignoring", left);
            continue;
        }

        if(mode == COMPILE && strncasecmp(argv[i], "--dest=", 7) == 0) {
            dest_path = &argv[i][7];
            continue;
        }

        if(mode == COMPILE && strcasecmp(argv[i], "--emit-ir") == 0) {
            emit_ir = true;
            continue;
        }

        log_warning("unrecognized flag '%s'", argv[i]);
    }

    if(source_count == 0) {
        log_warning("no sources");
        return EXIT_SUCCESS;
    }

    if(mode == RUN && optimization == CODEGEN_OPTIMIZATION_NONE) log_warning("run mode is unstable with zero optimizations");

    /* Parse: Source -> Tokens -> AST */
    ast_node_t *ast_root_nodes[source_count];
    memory_allocator_t *ast_allocator = memory_allocator_make();
    memory_active_allocator_set(ast_allocator);
    for(size_t i = 0; i < source_count; i++) {
        tokenizer_t *tokenizer = tokenizer_make(sources[i]);
        ast_root_nodes[i] = parser_root(tokenizer);
        tokenizer_free(tokenizer);
    }
    memory_active_allocator_set(NULL);

    /* Lower: AST -> IR */
    memory_allocator_t *ir_allocator = memory_allocator_make();
    memory_allocator_t *work_allocator = memory_allocator_make();
    memory_active_allocator_set(ir_allocator);

    ir_unit_t unit = { .root_namespace = IR_NAMESPACE_INIT };
    ir_type_cache_t *type_cache = ir_type_cache_make();

    pass_lower_context_t *lower_context = pass_lower_context_make(work_allocator, &unit, type_cache);
    for(size_t i = 0; i < source_count; i++) pass_lower_populate_modules(lower_context, ast_root_nodes[i]);
    for(size_t i = 0; i < source_count; i++) pass_lower_populate_types(lower_context, ast_root_nodes[i]);
    for(size_t i = 0; i < source_count; i++) pass_lower_populate_final(lower_context, ast_root_nodes[i]);
    for(size_t i = 0; i < source_count; i++) pass_lower(lower_context, ast_root_nodes[i]);

    memory_allocator_free(work_allocator);
    memory_allocator_free(ast_allocator);

    /* Passes: IR -> IR */
    pass_eval_types(&unit, type_cache);

    /* Codegen: IR -> LLVM_IR */
    codegen_context_t *cg_context = codegen(&unit, type_cache, optimization, code_model, reloc_mode, features);

    int ret = EXIT_SUCCESS;
    switch(mode) {
        case COMPILE: {
            if(dest_path == NULL) dest_path = add_extension(source_count > 0 ? "out" : sources[0]->name, emit_ir ? "ll" : "o", '.');
            codegen_emit(cg_context, dest_path, emit_ir);
            break;
        }
        case RUN: ret = codegen_run(cg_context); break;
    }

    /* Cleanup */
    codegen_dispose_context(cg_context);
    memory_active_allocator_set(NULL);
    memory_allocator_free(ir_allocator);

    return ret;
}
