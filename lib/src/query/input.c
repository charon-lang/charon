#include "input.h"

#include "common/text.h"
#include "core/db.h"
#include "core/interner.h"
#include "core/store.h"
#include "query/queries.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const text_t *intern_text(db_t *db, const char *src, size_t len) {
    text_t *scratch = malloc(sizeof(text_t) + len);
    scratch->size = len;
    memcpy(scratch->data, src, len);

    const text_t *interned = interner_intern(db->text_interner, scratch);
    free(scratch);
    return interned;
}

static uint64_t size_file(const void *value) {
    (void) value;
    return sizeof(file_t);
}

static uint64_t hash_file(const void *value, size_t value_size) {
    (void) value_size;
    const file_t *file = value;
    return text_hash(file->path);
}

static bool equal_file(const void *a, const void *b, size_t value_size) {
    (void) value_size;
    return text_equal(((const file_t *) a)->path, ((const file_t *) b)->path);
}

static query_input_t file_input(const file_t *file) {
    return (query_input_t) { .kind = QUERY_INPUT_FILE, .ptr = file };
}

/// Intern a file_t for path, reserving its store handle the first time it is seen
static const file_t *ensure_file(db_t *db, const char *path) {
    file_t candidate = {
        .path = intern_text(db, path, strlen(path)),
        .handle = { .index = 0, .revision = 0 },
    };

    const file_t *file = interner_intern(db->file_interner, &candidate);

    if(file->handle.revision == 0) {
        store_handle_t handle = store_insert(db->store, (void *) file, query_drop_none);
        ((file_t *) file)->handle = handle;
    }

    return file;
}

const file_t *db_add_file_text(db_t *db, const char *path, const char *src, size_t len) {
    const file_t *file = ensure_file(db, path);
    db_set_file_source(db, file, src, len);
    return file;
}

const file_t *db_add_file(db_t *db, const char *path) {
    FILE *stream = fopen(path, "rb");
    if(stream == nullptr) {
        fprintf(stderr, "charon: open source file '%s' (%s)\n", path, strerror(errno));
        return nullptr;
    }

    if(fseek(stream, 0, SEEK_END) != 0) {
        fprintf(stderr, "charon: seek source file '%s' (%s)\n", path, strerror(errno));
        fclose(stream);
        return nullptr;
    }

    long length = ftell(stream);
    if(length < 0) {
        fprintf(stderr, "charon: size source file '%s' (%s)\n", path, strerror(errno));
        fclose(stream);
        return nullptr;
    }

    rewind(stream);

    char *buffer = malloc(length == 0 ? 1 : (size_t) length);
    size_t read = fread(buffer, 1, (size_t) length, stream);
    fclose(stream);

    if(read != (size_t) length) {
        fprintf(stderr, "charon: read source file '%s'\n", path);
        free(buffer);
        return nullptr;
    }

    const file_t *file = db_add_file_text(db, path, buffer, (size_t) length);
    free(buffer);
    return file;
}

void db_set_file_source(db_t *db, const file_t *file, const char *src, size_t len) {
    const text_t *interned = intern_text(db, src, len);
    query_input_t input = file_input(file);
    query_set_input(db, QUERY_FILE_TEXT, &input, (void *) interned, query_drop_none);
}

const text_t *db_file_source(db_t *db, const file_t *file) {
    query_input_t input = file_input(file);
    return query_execute(db, QUERY_FILE_TEXT, &input);
}

const file_t *db_file_from_handle(db_t *db, store_handle_t handle) {
    if(handle.revision == 0) return nullptr;
    return store_get(db->store, handle);
}

void query_input_init(db_t *db) {
    db->file_interner = interner_new(0, 0, size_file, hash_file, equal_file);
}

void query_input_deinit(db_t *db) {
    interner_destroy(db->file_interner);
}
