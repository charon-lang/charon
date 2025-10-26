#include "charon/token.h"
#include "io.h"
#include "json_object.h"
#include "message.h"

#include <assert.h>
#include <charon/element.h>
#include <charon/lexer.h>
#include <charon/source.h>
#include <json.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static charon_lexer_token_t *g_tokens = nullptr;
static size_t g_token_count = 0;

static void lsp_log(const char *fmt, ...) {
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

int main(int argc, char **argv) {
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    bool shutdown = false;
    bool running = true;
    while(running) {
        struct json_object *msg = io_read_message(stdin);
        if(msg == nullptr) {
            io_write_message_response_error(stdout, NULL, -32700, "Parse error");
            continue;
        }

        if(message_has_method(msg, "initialize")) {
            struct json_object *id = NULL;
            json_object_object_get_ex(msg, "id", &id);

            struct json_object *cap = json_object_new_object();
            json_object_object_add(cap, "textDocumentSync", json_object_new_int(1));

            struct json_object *hoverProvider = json_object_new_boolean(1);
            json_object_object_add(cap, "hoverProvider", hoverProvider);

            struct json_object *serverInfo = json_object_new_object();
            json_object_object_add(serverInfo, "name", json_object_new_string("charonlsp"));
            json_object_object_add(serverInfo, "version", json_object_new_string("0.1"));

            struct json_object *result = json_object_new_object();
            json_object_object_add(result, "capabilities", cap);
            json_object_object_add(result, "serverInfo", serverInfo);

            io_write_message_response_result(stdout, id, result);
        } else if(message_has_method(msg, "initialized")) {
            lsp_log("Server initialized");
        } else if(message_has_method(msg, "shutdown")) {
            shutdown = true;

            struct json_object *id = NULL;
            json_object_object_get_ex(msg, "id", &id);
            io_write_message_response_result(stdout, id, json_object_new_null());
        } else if(message_has_method(msg, "exit")) {
            running = false;
        } else if(message_has_method(msg, "textDocument/didOpen")) {
            struct json_object *params = json_object_object_get(msg, "params");
            struct json_object *text_doc = json_object_object_get(params, "textDocument");
            struct json_object *uri = json_object_object_get(text_doc, "uri");
            lsp_log("Open(%s)\n", json_object_get_string(uri));
        } else if(message_has_method(msg, "textDocument/didChange")) {
            struct json_object *params = json_object_object_get(msg, "params");
            struct json_object *text_doc = json_object_object_get(params, "textDocument");
            struct json_object *uri = json_object_object_get(text_doc, "uri");
            lsp_log("Change(%s)\n", json_object_get_string(uri));

            struct json_object *changes = json_object_object_get(params, "contentChanges");
            struct json_object *first_change = json_object_array_get_idx(changes, 0);
            struct json_object *text = json_object_object_get(first_change, "text");

            const char *data = json_object_get_string(text);

            //
            charon_source_t *source = charon_source_make("test", data, strlen(data));
            charon_lexer_t *lexer = charon_lexer_make(source);

            if(g_tokens != nullptr) free(g_tokens);
            g_tokens = nullptr;
            g_token_count = 0;
            while(!charon_lexer_is_eof(lexer)) {
                g_tokens = reallocarray(g_tokens, ++g_token_count, sizeof(charon_lexer_token_t));
                g_tokens[g_token_count - 1] = charon_lexer_advance(lexer);
            }

            charon_lexer_destroy(lexer);
            charon_source_destroy(source);
        } else if(message_has_method(msg, "textDocument/hover")) {
            struct json_object *id = NULL;
            json_object_object_get_ex(msg, "id", &id);

            struct json_object *params = json_object_object_get(msg, "params");
            struct json_object *position = json_object_object_get(params, "position");

            uint64_t line = json_object_get_uint64(json_object_object_get(position, "line"));
            uint64_t column = json_object_get_uint64(json_object_object_get(position, "character"));

            charon_token_kind_t token_kind = CHARON_TOKEN_KIND_UNKNOWN;
            for(size_t i = 0; i < g_token_count; i++) {
                if(g_tokens[i].line != line) continue;
                if(column < g_tokens[i].column || column > g_tokens[i].column + g_tokens[i].length) continue;
                token_kind = g_tokens[i].kind;
                break;
            }

            struct json_object *contents = json_object_new_object();
            json_object_object_add(contents, "kind", json_object_new_string("plaintext"));
            json_object_object_add(contents, "value", json_object_new_string(charon_token_kind_tostring(token_kind)));

            struct json_object *result = json_object_new_object();
            json_object_object_add(result, "contents", contents);

            io_write_message_response_result(stdout, id, result);
        } else {
            if(message_is_request(msg)) {
                struct json_object *id = NULL;
                json_object_object_get_ex(msg, "id", &id);
                io_write_message_response_error(stdout, id, -32601, "Method not found");
            }
        }

        json_object_put(msg);
    }

    return shutdown ? 0 : 1;
}
