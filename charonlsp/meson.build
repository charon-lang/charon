cgo_env = environment()
cgo_env.set('CGO_CFLAGS', ' '.join([
    '-std=gnu23',
    '-I' + meson.project_source_root() / 'lib/include',
    '-I' + meson.project_source_root() / 'lib/src'
]))
cgo_env.set('CGO_LDFLAGS', ' '.join([
    '-L', dependency_pcre.get_variable('libdir'),
    '-L', dependency_llvm.get_variable('libdir'),
    '-l', dependency_pcre.name().strip('lib'),
    '-l', dependency_llvm.name().strip('lib'),
    charon_lib.full_path()
]))

go_target = custom_target('charonlsp',
    output: 'charonlsp',
    input: [
        'lsp.go',
        'bindings.go',
        'document.go',
    ],
    command: ['go', 'build', '-C', meson.current_source_dir(), '-o', meson.current_build_dir() / '@OUTPUT@', '.'],
    env: cgo_env,
    build_by_default: true
)
