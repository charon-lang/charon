#pragma once

#include <json.h>
#include <stdio.h>

/*
 * Read a message.
 * Returns the message as a json object or nullptr.
 */
struct json_object *io_read_message(FILE *f);

bool io_write_message_response_error(FILE *f, struct json_object *id, int code, const char *message);
bool io_write_message_response_result(FILE *f, struct json_object *id, struct json_object *result_owned);
bool io_write_message_notification(FILE *f, const char *method, struct json_object *params_owned);
