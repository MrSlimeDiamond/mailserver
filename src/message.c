#include "message.h"
#include <json.h>

json_object *message_to_json(message message) {
    json_object *root = json_object_new_object();

    json_object_object_add(root, "id", json_object_new_int(message.id));
    json_object_object_add(root, "timestamp", json_object_new_int(message.time));
    json_object_object_add(root, "from", user_to_json(message.from));
    json_object_object_add(root, "to", user_to_json(message.to));
    json_object_object_add(root, "content", json_object_new_string(message.content));
    json_object_object_add(root, "read", json_object_new_boolean(message.read));

    return root;
}