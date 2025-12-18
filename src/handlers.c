#include "handlers.h"
#include "server.h"
#include "storage.h"
#include "logging.h"
#include <json.h>
#include <stdbool.h>

json_object *create_root_json_object(int status, char *request_type) {
    json_object *root = json_object_new_object();
    json_object_object_add(root, "status", json_object_new_int(status));
    json_object_object_add(root, "request_type", json_object_new_string(request_type));
    return root;
}

void send_internal_server_error(int conn_id) {
    char *buffer = "{\"status\":500,\"message\":\"Internal server error\"}\n";
    send(conn_id, buffer, strlen(buffer), 0);
}

json_object *get_data(int conn_id, json_object *root) {
    json_object *data = json_object_object_get(root, "data");

    //log_debug("data: %s", data);

    if (!data) {
        // log_debug("Invalid data AT THIS POINT HERE HI");
        send_invalid_request(conn_id);
        return NULL;
    } else {
        return data;
    }
}

void handle_status_request(int conn_id) {
    json_object *root = create_root_json_object(200, "status");

    // Create "data" object
    json_object *data = json_object_new_object();
    
    // Add fields to "data"
    json_object_object_add(data, "up_since", json_object_new_int64(start_time));
    json_object_object_add(data, "connections", json_object_new_int(connection_count));

    // Add "data" to root
    json_object_object_add(root, "data", data);

    send_to_client(conn_id, root);

    json_object_put(root); // free the memory
}

void handle_user_id_request(int conn_id, json_object *root) {
    json_object *data = get_data(conn_id, root);
    if (!data) return;

    json_object *name_object = json_object_object_get(data, "name");
    if (!json_object_is_type(name_object, json_type_string)) {
        send_invalid_request(conn_id);
        return;
    }

    char *name = json_object_get_string(name_object);

    int id = get_user_id(name);

    json_object *res_root = create_root_json_object(200, "user_id");
    json_object *res_data = json_object_new_object();
    json_object_object_add(res_data, "name", json_object_new_string(name));
    json_object_object_add(res_data, "id", json_object_new_int(id));
    json_object_object_add(res_root, "data", res_data);

    send_to_client(conn_id, res_root);
}

static user *resolve_user(int conn_id, json_object *user_object) {
    user *resolved_user = malloc(sizeof(*resolved_user));
    if (!resolved_user) {
        send_internal_server_error(conn_id);
        return NULL;
    }

    resolved_user->id = -1;
    resolved_user->name = NULL;

    json_object *name_object = json_object_object_get(user_object, "name");
    json_object *id_object   = json_object_object_get(user_object, "id");

    if (!name_object && !id_object) {
        send_invalid_request(conn_id);
        free(resolved_user);
        return NULL;
    }

    if (name_object) {
        const char *json_name = json_object_get_string(name_object);
        resolved_user->name = strdup(json_name);
        if (!resolved_user->name) {
            send_internal_server_error(conn_id);
            free(resolved_user);
            return NULL;
        }
    }

    if (id_object) {
        resolved_user->id = json_object_get_int(id_object);
    }

    if (resolved_user->id == -1 && resolved_user->name) {
        int fetched_id = get_user_id(resolved_user->name);
        if (fetched_id == -1) {
            send_internal_server_error(conn_id);
            free(resolved_user->name);
            free(resolved_user);
            return NULL;
        }

        resolved_user->id = fetched_id;
        json_object_object_add(
            user_object,
            "id",
            json_object_new_int(resolved_user->id)
        );
    }

    if (!resolved_user->name && resolved_user->id != -1) {
        char *fetched_name = get_user_name(resolved_user->id);
        if (!fetched_name) {
            send_internal_server_error(conn_id);
            free(resolved_user);
            return NULL;
        }

        resolved_user->name = fetched_name;
        json_object_object_add(
            user_object,
            "name",
            json_object_new_string(resolved_user->name)
        );
    }

    return resolved_user;
}

void handle_submit_message_request(int conn_id, json_object *root) {
    json_object_object_add(root, "status", json_object_new_int(200));
    json_object *data = get_data(conn_id, root);

    if (!data) {
        send_invalid_request(conn_id);
        log_debug("No data");
        return;
    }

    json_object *from_object = json_object_object_get(data, "from");
    json_object *to_object = json_object_object_get(data, "to");
    json_object *content_object = json_object_object_get(data, "content");

    if (!from_object || !to_object || !content_object) {
        send_invalid_request(conn_id);
        return;
    }

    user *from_user = resolve_user(conn_id, from_object);
    if (!from_user) {
        return;
    }

    user *to_user = resolve_user(conn_id, to_object);
    if (!to_user) {
        free(from_user->name);
        free(from_user);
        return;
    }

    const char *content = json_object_get_string(content_object);
    if (!content || !*content) {
        send_invalid_request(conn_id);
        goto cleanup;
    }

    message msg = {
        .from = *from_user,
        .to = *to_user,
        .content = content
    };

    log_debug("Submitting!");
    int id = submit_message(msg);

    log_debug("Message ID: %d", id);

    json_object_object_add(data, "timestamp", json_object_new_int(time(NULL)));
    json_object_object_add(data, "id", json_object_new_int(id));
    json_object_object_add(data, "read", json_object_new_boolean(0));

    broadcast_json(root);
cleanup:
    free(from_user->name);
    free(from_user);
    free(to_user->name);
    free(to_user);
}

void handle_get_messages_request(int conn_id, json_object *root) {
    json_object_object_add(root, "status", json_object_new_int(200));
    json_object *data = get_data(conn_id, root);
    json_object *user_object = json_object_object_get(data, "user");

    if (!user_object) {
        send_invalid_request(conn_id);
        return;
    }

    bool read_messages_only = false;

    json_object *unread_only_object = json_object_object_get(data, "unread_messages_only");
    if (unread_only_object) {
        if (!json_object_is_type(unread_only_object, json_type_boolean)) {
            send_invalid_request(conn_id);
            return;
        }
        read_messages_only = json_object_get_boolean(unread_only_object);
    }
    
    char *name;
    int id;

    json_object *name_object = json_object_object_get(user_object, "name");
    json_object *id_object = json_object_object_get(user_object, "id");

    if (!name_object && !id_object) {
        send_invalid_request(conn_id);
        return;
    }

    if (name_object) {
        name = json_object_get_string(name_object);
        id = get_user_id(name);
    } else {
        id = json_object_get_int(id_object);
        name = get_user_name(id);

        if (!name) {
            char *msg = "{\"status\":404,\"message\":\"User not found\"}\n";
            send(conn_id, msg, sizeof(msg), 0);
            return;
        }
    }

    // array for all of the messages
    json_object *messages_json = json_object_new_array();

    user user = { .id = id, .name = name };

    int message_count;
    message *messages = get_messages(user, read_messages_only, &message_count);
    for (int i = 0; i < message_count; i++) {
        message message = messages[i];
        json_object *message_json = message_to_json(message);
        json_object_array_add(messages_json, message_json);
    }

    // get_messages uses malloc
    free(messages);

    // add the array of messages to the data object
    json_object_object_add(data, "messages", messages_json);

    send_to_client(conn_id, root);
}

void handle_set_read_request(int conn_id, json_object *root) {
    json_object *data = get_data(conn_id, root);
    if (!data) return;

    json_object *id_object = json_object_object_get(data, "id");
    if (!id_object || !json_object_is_type(id_object, json_type_int)) {
        send_invalid_request(conn_id);
        return;
    }

    int id = json_object_get_int(id_object);
    if (mark_read(id) == 0) {
        send_internal_server_error(conn_id);
        return;
    }
    json_object_object_add(root, "status", json_object_new_int(200));
    send_to_client(conn_id, root);
}

void handle_get_next_request(int conn_id, json_object *root) {
    json_object *data = get_data(conn_id, root);
    if (!data) return;

    json_object *mark_read_object = json_object_object_get(root, "mark_read");

    bool mark_read = false;

    if (mark_read_object && json_object_is_type(mark_read_object, json_type_boolean)) {
        mark_read = json_object_get_boolean(mark_read_object);
    }

    json_object *user_object = json_object_object_get(data, "user");
    user *user = resolve_user(conn_id, user_object);
    if (!user) return;
    
    message *message = get_next(*user, mark_read);
    json_object *messages_json = json_object_new_array();

    if (message) {
        json_object *message_json = message_to_json(*message);
        json_object_array_add(messages_json, message_json);

        free(message);
    }

    free(user);

    // add the array of messages to the data object
    json_object_object_add(data, "messages", messages_json);

    send_to_client(conn_id, root);
}