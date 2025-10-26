#include "message.h"

#include <json.h>
#include <stdlib.h>
#include <string.h>

bool message_is_request(const struct json_object *message) {
    return json_object_object_get_ex(message, "id", NULL);
}

bool message_has_method(const struct json_object *message, const char *method) {
    struct json_object *message_method;
    if(!json_object_object_get_ex(message, "method", &message_method)) return false;
    if(!json_object_is_type(message_method, json_type_string)) return false;
    return strcmp(json_object_get_string(message_method), method) == 0;
}
