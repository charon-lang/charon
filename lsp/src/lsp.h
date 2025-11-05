#pragma once

#include <json.h>

#define LSP_REGISTER_MESSAGE_HANDLER(METHOD, HANDLER) static const lsp_message_handler_t lsp_message_handler_##HANDLER [[gnu::used, gnu::section("lsp_handlers")]] = { .method = METHOD, .handler = HANDLER };

typedef struct {
    const char *method;
    void (*handler)(struct json_object *message);
} lsp_message_handler_t;

extern bool g_lsp_exit_code;
extern bool g_lsp_running;

void lsp_log(const char *fmt, ...);
