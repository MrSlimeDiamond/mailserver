#pragma once

#include "user.h"
#include <time.h>
#include <json.h>
#include <stdbool.h>

typedef struct message {
    int id;
    time_t time;
    user from;
    user to;
    char *content;
    bool read;
} message;

json_object *message_to_json(message message);