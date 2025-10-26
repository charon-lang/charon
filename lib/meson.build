dependency_pcre = dependency('libpcre2-8')
dependency_llvm = dependency('llvm')

charon_lib_includes = include_directories('include')

charon_lib = static_library(
    'charon',
    files(
        'src/memory.c',
        'src/element.c',
        'src/lexer.c',
        'src/node.c',
        'src/source.c',
        'src/token.c',
    ),
    dependencies: [dependency_pcre, dependency_llvm],
    include_directories: [include_directories('src'), charon_lib_includes],
    c_args: [ '-DPCRE2_CODE_UNIT_WIDTH=8' ] + run_command('llvm-config', '--cflags', 'core', check: false).stdout().strip().split(),
    install: true
)
