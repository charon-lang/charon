#include "source.h"

#include "lib/log.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <sys/stat.h>

source_t *source_from_path(const char *path) {
    const char *name = strdup(basename(strdup(path)));

    FILE *file = fopen(path, "r");
    if(file == NULL) log_fatal("failed to open source file '%s' (%s)", name, strerror(errno));

    struct stat st;
    if(file == NULL || fstat(fileno(file), &st) != 0) log_fatal("failed to stat '%s' (%s)", name, strerror(errno));

    void *data = malloc(st.st_size);
    if(fread(data, 1, st.st_size, file) != st.st_size) log_fatal("failed to read source file '%s' (%s)", name, strerror(errno));
    if(fclose(file) != 0) log_warning("failed to close file '%s'", name);

    source_t *source = malloc(sizeof(source_t));
    source->name = name;
    source->data_buffer = data;
    source->buffer_size = st.st_size;
    return source;
}

void source_free(source_t *source) {
    free((void *) source->name);
    free((void *) source->data_buffer);
    free(source);
}