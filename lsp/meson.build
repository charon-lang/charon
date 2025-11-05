jsonc = dependency('json-c', fallback: ['json-c', 'json_c_dep'])

executable(
    'charonlsp',
    files(
        'src/messages/text_document.c',
        'src/messages/lifecycle.c',
        'src/document.c',
        'src/io.c',
        'src/linedb.c',
        'src/lsp.c',
        'src/main.c'
    ),
    include_directories: [include_directories('src'), charon_lib_includes],
    link_with: [charon_lib],
    dependencies : [jsonc],
    install: true
)
