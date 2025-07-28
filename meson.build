project('charon', 'c', 'cpp')

sources = files(
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
)

dependencies = [
    dependency('libpcre2-8'),
    dependency('llvm'),
    dependency('libcjson')
]

flags = [
    '-xc',
    '-std=gnu23',
    '-DPCRE2_CODE_UNIT_WIDTH=8',
    '-Werror',
    '-Wswitch',
    '-Wimplicit-fallthrough',
    '-Wall',
    '-O3'
]

if get_option('buildtype') == 'release'
    flags += ['-O3']
else
    flags += ['-O1', '-g']
endif

flags += run_command('llvm-config', '--cflags', 'core', check: false).stdout().strip().split()

llvm_modules = ['core', 'analysis', 'executionengine']

includes = include_directories('src')

executable('charon', 'src/main.c', sources, dependencies: dependencies, include_directories: includes, c_args: flags)
executable('lsp', 'src/lsp.c', sources, dependencies: dependencies, include_directories: includes, c_args: flags)
