project('charon', 'c', 'cpp', default_options : ['c_std=gnu23', 'warning_level=1', 'werror=true'])

add_project_arguments(
    get_option('buildtype') == 'release' ? '-O3' : '-O1',
    '-Wswitch',
    '-Wimplicit-fallthrough',
    language: 'c'
)

subdir('lib')
subdir('charonc')
subdir('charonlsp')
subdir('charontest')
