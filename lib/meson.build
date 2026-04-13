dependency_llvm = dependency('llvm')

charon_lib_includes = include_directories('include')

charon_lib = static_library(
    'charon',
    files('src/lexer.c', 'src/syntax/token.c', 'src/syntax/trivia.c'),
    dependencies: [dependency_llvm],
    include_directories: [include_directories('src'), charon_lib_includes],
    c_args: run_command('llvm-config', '--cflags', 'core', check: false).stdout().strip().split(),
    install: true,
)
