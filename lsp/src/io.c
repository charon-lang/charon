#include "io.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

static bool read_exact(FILE *f, char *buf, size_t len) {
    size_t got = 0;
    while(got < len) {
        // TODO: kind of questionable whether we want to return on nothing read
        size_t n = fread(buf + got, 1, len - got, f);
        if(n == 0) {
            if(ferror(f)) return false;

            // EOF
            return false;
        }
        got += n;
    }
    return true;
}

static bool read_message_raw(FILE *f, char **out_json, size_t *out_len) {
    size_t content_length = -1;

    char line[4096];
    for(;;) {
        if(fgets(line, sizeof(line) - 1, f) == nullptr) return false;

        size_t i = strlen(line);
        while(i > 0 && (line[i - 1] == '\r' || line[i - 1] == '\n')) line[--i] = '\0';
        if(i == 0) break;

        if(strncasecmp(line, "Content-Length:", 15) == 0) {
            char *content = line + 15;
            while(isspace(*content)) content++;

            errno = 0;
            long value = strtol(content, nullptr, 10);
            if(errno == 0 && value >= 0) content_length = value;
        }
    }

    char *buf = malloc(content_length + 1);
    if(!read_exact(f, buf, content_length)) {
        free(buf);
        return false;
    }
    buf[content_length] = '\0';

    *out_json = buf;
    *out_len = content_length;

    return true;
}

static bool write_all(FILE *f, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *) buf;
    size_t off = 0;
    while(off < len) {
        size_t n = fwrite(p + off, 1, len - off, f);
        if(n == 0) return false;
        off += n;
    }
    return fflush(f) == 0;
}

static bool write_raw(FILE *f, const char *json_str) {
    char header[128];
    size_t body_len = strlen(json_str);
    int n = snprintf(header, sizeof(header), "Content-Length: %zu\r\n\r\n", body_len);
    if(n < 0 || n >= sizeof(header)) return false;
    if(!write_all(f, header, n)) return false;
    if(!write_all(f, json_str, body_len)) return false;
    return true;
}

struct json_object *io_read_message(FILE *f) {
    char *raw = nullptr;
    size_t raw_len = 0;
    if(!read_message_raw(stdin, &raw, &raw_len)) return nullptr;
    struct json_object *message = json_tokener_parse(raw);
    free(raw);
    return message;
}

bool io_write_message_response_error(FILE *f, struct json_object *id, int code, const char *message) {
    struct json_object *root = json_object_new_object();
    json_object_object_add(root, "jsonrpc", json_object_new_string("2.0"));
    if(id) json_object_object_add(root, "id", json_object_get(id)); // echo id
    struct json_object *err = json_object_new_object();
    json_object_object_add(err, "code", json_object_new_int(code));
    json_object_object_add(err, "message", json_object_new_string(message));
    json_object_object_add(root, "error", err);

    const char *s = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if(!write_raw(f, s)) return false;
    json_object_put(root);
    return true;
}

bool io_write_message_response_result(FILE *f, struct json_object *id, struct json_object *result_owned) {
    struct json_object *root = json_object_new_object();
    json_object_object_add(root, "jsonrpc", json_object_new_string("2.0"));
    if(id) json_object_object_add(root, "id", json_object_get(id));
    json_object_object_add(root, "result", result_owned); // takes ownership

    const char *s = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if(!write_raw(f, s)) return false;
    json_object_put(root);
    return true;
}

bool io_write_message_notification(FILE *f, const char *method, struct json_object *params_owned) {
    struct json_object *root = json_object_new_object();
    json_object_object_add(root, "jsonrpc", json_object_new_string("2.0"));
    json_object_object_add(root, "method", json_object_new_string(method));
    if(params_owned) json_object_object_add(root, "params", params_owned);

    const char *s = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if(!write_raw(f, s)) return false;
    json_object_put(root);
    return true;
}
