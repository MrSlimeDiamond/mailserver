#pragma once
#include <json.h>

typedef struct user {
    int id;
    char *name;
} user;

json_object *user_to_json(user user);