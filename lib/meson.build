dependency_pcre = dependency('libpcre2-8')
dependency_llvm = dependency('llvm')

charon_lib_includes = include_directories('include')

charon_lib = static_library(
    'charon',
    files(
        'src/common/platform.c',
        'src/common/memory.c',
        'src/common/utf8.c',
        'src/common/list.c',
        'src/parser/expression.c',
        'src/parser/parser.c',
        'src/parser/root.c',
        'src/parser/statement.c',
        'src/parser/tlc.c',
        'src/parser/type.c',
        'src/sema/queries/symtab.c',
        'src/sema/context.c',
        'src/sema/query.c',
        'src/diag.c',
        'src/element.c',
        'src/lexer.c',
        'src/node.c',
        'src/query.c',
        'src/token.c',
        'src/trivia.c',
        'src/util.c',
        'src/test.c',
    ),
    dependencies: [dependency_pcre, dependency_llvm],
    include_directories: [include_directories('src'), charon_lib_includes],
    c_args: [ '-DPCRE2_CODE_UNIT_WIDTH=8' ] + run_command('llvm-config', '--cflags', 'core', check: false).stdout().strip().split(),
    install: true
)
