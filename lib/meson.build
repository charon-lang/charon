dependency_pcre = dependency('libpcre2-8')
dependency_llvm = dependency('llvm')

charon_lib_includes = include_directories('include')

charon_lib = static_library(
    'charon',
    files(
        'src/ast/function.c',
        'src/ast/module.c',
        'src/common/list.c',
        'src/common/memory.c',
        'src/common/utf8.c',
        'src/core/context.c',
        'src/core/diag.c',
        'src/core/interner.c',
        'src/core/query.c',
        'src/element.c',
        'src/lexer.c',
        'src/node.c',
        'src/parser/expression.c',
        'src/parser/parser.c',
        'src/parser/root.c',
        'src/parser/statement.c',
        'src/parser/tlc.c',
        'src/parser/type.c',
        'src/platform.c',
        'src/sema/queries/symtab.c',
        'src/test.c',
        'src/token.c',
        'src/trivia.c',
        'src/util.c',
    ),
    dependencies: [dependency_pcre, dependency_llvm],
    include_directories: [include_directories('src'), charon_lib_includes],
    c_args: ['-DPCRE2_CODE_UNIT_WIDTH=8']
    + run_command('llvm-config', '--cflags', 'core', check: false).stdout().strip().split(),
    install: true,
)
