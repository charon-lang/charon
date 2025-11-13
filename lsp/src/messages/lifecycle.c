#include "io.h"
#include "json_object.h"
#include "lsp.h"

#include <assert.h>
#include <charon/lexer.h>
#include <charon/parser.h>
#include <json.h>
#include <stddef.h>
#include <string.h>

static void handler_initialize(struct json_object *message) {
    struct json_object *id = NULL;
    json_object_object_get_ex(message, "id", &id);

    // Parse client caps
    struct json_object *params = json_object_object_get(message, "params");
    struct json_object *capabilities = json_object_object_get(params, "capabilities");
    struct json_object *general_capabilities = json_object_object_get(capabilities, "general");
    struct json_object *pos_encodings = json_object_object_get(general_capabilities, "positionEncodings");

    for(size_t i = 0; i < json_object_array_length(pos_encodings); i++) {
        lsp_log("supported position encoding: %s", json_object_get_string(json_object_array_get_idx(pos_encodings, i)));
    }

    // Construct our response
    struct json_object *cap = json_object_new_object();
    json_object_object_add(cap, "textDocumentSync", json_object_new_int(2));

    struct json_object *hover_rovider = json_object_new_boolean(true);
    json_object_object_add(cap, "hoverProvider", hover_rovider);

    struct json_object *server_info = json_object_new_object();
    json_object_object_add(server_info, "name", json_object_new_string("charonlsp"));
    json_object_object_add(server_info, "version", json_object_new_string("0.1"));

    struct json_object *result = json_object_new_object();
    json_object_object_add(result, "capabilities", cap);
    json_object_object_add(result, "serverInfo", server_info);

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
