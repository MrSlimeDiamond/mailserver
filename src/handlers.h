#pragma once
#include <sys/socket.h>
#include <string.h>
#include <json.h>

void handle_status_request(int conn_id);

void handle_user_id_request(int conn_id, json_object *root);

void handle_submit_message_request(int conn_id, json_object *root);

void handle_get_messages_request(int conn_id, json_object *root);

void handle_set_read_request(int conn_id, json_object *root);

void handle_get_next_request(int conn_id, json_object *root);