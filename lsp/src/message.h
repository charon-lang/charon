#pragma once

#include <json.h>

/**
 * Check whether the message is a request.
 */
bool message_is_request(const struct json_object *message);

/*
 * Check whether the message has a specific method.
 */
bool message_has_method(const struct json_object *message, const char *method);
