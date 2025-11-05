#include "io.h"
#include "lsp.h"

#include <json.h>
#include <string.h>

extern const lsp_message_handler_t __start_lsp_handlers[];
extern const lsp_message_handler_t __stop_lsp_handlers[];

int main(int argc, char **argv) {
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    while(g_lsp_running) {
        struct json_object *message = io_read_message(stdin);
        if(message == nullptr) {
            io_write_message_response_error(stdout, NULL, -32700, "Parse error");
            continue;
        }

        struct json_object *message_method;
        json_object_object_get_ex(message, "method", &message_method);
        json_object_is_type(message_method, json_type_string);
        const char *method = json_object_get_string(message_method);

        for(const lsp_message_handler_t *handler = __start_lsp_handlers; handler < __stop_lsp_handlers; ++handler) {
            if(strcmp(handler->method, method) != 0) continue;
            handler->handler(message);
            goto next;
        }

        if(json_object_object_get_ex(message, "id", NULL)) {
            struct json_object *id = NULL;
            json_object_object_get_ex(message, "id", &id);
            io_write_message_response_error(stdout, id, -32601, "Method not found");
        }

    next:
        json_object_put(message);
    }

    return g_lsp_exit_code;
}
