#pragma once

#include <time.h>
#include <json.h>

extern int *connections;
extern int connection_count;
extern time_t start_time;

void server_start(char *address, int port);

void send_invalid_request(int conn_id);

void send_to_client(int conn_id, json_object *json);

void broadcast_json(json_object *json);

void server_stop(void);