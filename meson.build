project('charon', 'c', 'cpp', default_options : ['c_std=gnu23', 'warning_level=1', 'werror=true'])

add_project_arguments(
    get_option('buildtype') == 'release' ? '-O3' : '-O1',
    '-Wswitch',
    '-Wimplicit-fallthrough',
    language: 'c'
)

traces = get_option('traces')

foreach t : traces
    macro_suffix = t.to_upper().replace('-', '_').replace('.', '_').replace(' ', '_')
    add_project_arguments('-DTRACE_' + macro_suffix, language: 'c')
endforeach

subdir('lib')

if get_option('build_lsp')
    subdir('lsp')
endif

subdir('compiler')
