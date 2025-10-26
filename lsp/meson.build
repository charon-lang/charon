jsonc = dependency('json-c', fallback: ['json-c', 'json_c_dep'])

executable(
    'charonlsp',
    files(
        'src/io.c',
        'src/main.c',
        'src/message.c'
    ),
    include_directories: [charon_lib_includes],
    link_with: [charon_lib],
    dependencies : [jsonc],
    install: true
)
