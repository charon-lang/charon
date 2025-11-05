#include "io.h"
#include "lsp.h"

#include <charon/lexer.h>
#include <charon/parser.h>
#include <json.h>
#include <string.h>

static void handler_initialize(struct json_object *message) {
    struct json_object *id = NULL;
    json_object_object_get_ex(message, "id", &id);

    struct json_object *cap = json_object_new_object();
    json_object_object_add(cap, "textDocumentSync", json_object_new_int(2));

    struct json_object *hoverProvider = json_object_new_boolean(true);
    json_object_object_add(cap, "hoverProvider", hoverProvider);

    struct json_object *serverInfo = json_object_new_object();
    json_object_object_add(serverInfo, "name", json_object_new_string("charonlsp"));
    json_object_object_add(serverInfo, "version", json_object_new_string("0.1"));

    struct json_object *result = json_object_new_object();
    json_object_object_add(result, "capabilities", cap);
    json_object_object_add(result, "serverInfo", serverInfo);

    io_write_message_response_result(stdout, id, result);
}

static void handler_initialized(struct json_object *message) {
    lsp_log("Server initialized");
}

static void handler_shutdown(struct json_object *message) {
    g_lsp_exit_code = 0;
    lsp_log("Server shutting down");

    struct json_object *id = NULL;
    json_object_object_get_ex(message, "id", &id);
    io_write_message_response_result(stdout, id, json_object_new_null());
}

static void handler_exit(struct json_object *message) {
    g_lsp_running = false;
}

LSP_REGISTER_MESSAGE_HANDLER("initialize", handler_initialize);
LSP_REGISTER_MESSAGE_HANDLER("initialized", handler_initialized);
LSP_REGISTER_MESSAGE_HANDLER("shutdown", handler_shutdown);
LSP_REGISTER_MESSAGE_HANDLER("exit", handler_exit);
