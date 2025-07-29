dependency_pcre = dependency('libpcre2-8')
dependency_llvm = dependency('llvm')

charon_lib_includes = include_directories('include')
charon_lib = static_library(
    'charon',
    files(
        'src/ast/attribute.c',
        'src/ast/node.c',
        'src/ast/type.c',
        'src/codegen/llvm.c',
        'src/ir/namespace.c',
        'src/ir/scope.c',
        'src/ir/type.c',
        'src/ir/visit.c',
        'src/lexer/token.c',
        'src/lexer/tokenizer.c',
        'src/lib/context.c',
        'src/lib/diag.c',
        'src/lib/log.c',
        'src/lib/memory.c',
        'src/lib/source.c',
        'src/parser/expr.c',
        'src/parser/root.c',
        'src/parser/stmt.c',
        'src/parser/tlc.c',
        'src/parser/util.c',
        'src/pass/eval_types.c',
        'src/pass/lower.c',
    ),
    dependencies: [dependency_pcre, dependency_llvm],
    include_directories: [include_directories('src'), charon_lib_includes],
    c_args: [
        '-DPCRE2_CODE_UNIT_WIDTH=8'
    ] + run_command('llvm-config', '--cflags', 'core', check: false).stdout().strip().split(),
    install: true
)
