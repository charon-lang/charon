#include "lsp.h"

#include "io.h"

#include <json.h>
#include <stdarg.h>
#include <stdio.h>

bool g_lsp_exit_code = 1;
bool g_lsp_running = true;

void lsp_log(const char *fmt, ...) {
    va_list list;
    va_start(list);

    char *dest;
    vasprintf(&dest, fmt, list);

    struct json_object *p = json_object_new_object();
    json_object_object_add(p, "type", json_object_new_int(3));
    json_object_object_add(p, "message", json_object_new_string(dest));
    io_write_message_notification(stdout, "window/logMessage", p);

    free(dest);

    va_end(list);
}
