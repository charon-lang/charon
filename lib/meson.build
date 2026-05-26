dependency_llvm = dependency('llvm')

charon_lib_includes = include_directories('include')

charon_lib = static_library(
    'charon',
    files(
        'src/common/list.c',
        'src/common/text.c',
        'src/core/db.c',
        'src/core/interner.c',
        'src/core/query.c',
        'src/core/store.c',
        'src/lexer.c',
        'src/parser/expression.c',
        'src/parser/parser.c',
        'src/parser/root.c',
        'src/parser/statement.c',
        'src/parser/tlc.c',
        'src/parser/type.c',
        'src/syntax/element.c',
        'src/syntax/node.c',
        'src/syntax/token.c',
        'src/syntax/trivia.c',
    ),
    dependencies: [dependency_llvm],
    include_directories: [include_directories('src'), charon_lib_includes],
    c_args: run_command('llvm-config', '--cflags', 'core', check: false).stdout().strip().split(),
    install: true,
)
