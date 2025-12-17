#include "user.h"
#include "logging.h"

json_object *user_to_json(user user) {
    json_object *root = json_object_new_object();

    if (user.id) {
        json_object_object_add(root, "id", json_object_new_int(user.id));
    }
    if (user.name) {
        json_object_object_add(root, "name", json_object_new_string(user.name));
    }

    return root;
}