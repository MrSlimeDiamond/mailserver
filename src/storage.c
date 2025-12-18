#define _XOPEN_SOURCE // for strptime()
#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif
#include "storage.h"
#include <libpq-fe.h>
#include "config.h"
#include "logging.h"
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 1024

static PGconn *conn = NULL;

int exec(char *sql) {
    PGresult *res = PQexec(conn, sql);
    int result = 1;
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        log_error("SQL error: %s", PQerrorMessage(conn));
        result = 0;
    }
    PQclear(res);
    return result;
}

int storage_open(void) {
    log_info("Opening database storage");

    char *user = cfg_getstr(config, CONFIG_DBUSER);
    char *password = cfg_getstr(config, CONFIG_DBPASSWORD);
    char *host = cfg_getstr(config, CONFIG_DBHOST);
    char *dbname = cfg_getstr(config, CONFIG_DBNAME);
    int dbport = cfg_getint(config, CONFIG_DBPORT);

    char buffer[BUF_SIZE];
    snprintf(buffer, BUF_SIZE, "user=%s password=%s host=%s dbname=%s port=%d", user, password, host, dbname, dbport);

    conn = PQconnectdb(buffer);

    log_debug(buffer);

    if (PQstatus(conn) != CONNECTION_OK) {
        log_error("Unable to connect to database: %s", PQerrorMessage(conn));
        return 0;
    }

    // we should be successfully connected now

    int server_ver = PQserverVersion(conn);
    char *username = PQuser(conn);
    char *db_name = PQdb(conn);
    log_info("Connected to database!");
    log_debug("Server version: %d", server_ver);
    log_debug("User: %s", username);
    log_debug("Database name: %s", db_name);

    if (!exec("CREATE TABLE IF NOT EXISTS users ("
        "id bigserial NOT NULL, "
        "name varchar(255) NOT NULL UNIQUE, "
        "CONSTRAINT users_pk PRIMARY KEY (id))")) {
        log_warn("Unable to create users table");
    }

    if (!exec("CREATE TABLE IF NOT EXISTS messages ("
        "id bigserial NOT NULL, "
        "timestamp timestamp DEFAULT CURRENT_TIMESTAMP NOT NULL, "
        "from_id bigserial NOT NULL, "
        "to_id bigserial NOT NULL, "
        "content text NOT NULL, "
        "read boolean NOT NULL DEFAULT FALSE, "
        "CONSTRAINT messages_pk PRIMARY KEY (id), "
        "CONSTRAINT messages_users_fk_from FOREIGN KEY (from_id) REFERENCES users(id), "
        "CONSTRAINT messages_users_fk_to FOREIGN KEY (to_id) REFERENCES users(id))")) {
        log_warn("Unable to create messages table");
    }
    return 1;
}

void storage_close(void) {
    log_info("Closing database");
    PQfinish(conn);
}

int get_user_id(char *name) {
    const char *paramValues[1] = { name };
    PGresult *res = PQexecParams(conn,
        "WITH ins AS ("
        "    INSERT INTO users (name) "
        "    VALUES ($1) "
        "    ON CONFLICT (name) DO NOTHING "
        "    RETURNING id"
        ") "
        "SELECT id FROM ins "
        "UNION "
        "SELECT id FROM users WHERE name=$1;",
        1,
        NULL,
        paramValues,
        NULL,
        NULL,
        0
    );

    if (!res) {
        log_error("PQexecParams failed: %s", PQerrorMessage(conn));
        return -1;
    }

    if (PQntuples(res) > 0) {
        int id = atoi(PQgetvalue(res, 0, 0));
        PQclear(res);
        return id;
    }

    PQclear(res);
    return -1; // should not happen
}

char *get_user_name(int id) {
    char id_str[12]; // enough for 32-bit int
    snprintf(id_str, sizeof(id_str), "%d", id);

    const char *paramValues[1] = { id_str };
    PGresult *res = PQexecParams(conn,
        "SELECT name FROM users WHERE id = $1",
        1,
        NULL,
        paramValues,
        NULL,
        NULL,
        0
    );

    if (!res) {
        log_error("PQexecParams failed: %s", PQerrorMessage(conn));
        return NULL;
    }

    if (PQntuples(res) > 0) {
        char* result = PQgetvalue(res, 0, 0);
        PQclear(res);
        return result;
    }

    PQclear(res);
    return NULL; // should only happen if the user isn't found
}

int submit_message(message message) {
    log_debug("Submit message");
    // TODO: Probably pull the user ID in the SQL instead of beforehand,
    // but I'm a lazy fuckarse bastard :)
    // if (message.user.id == -1) {
    //     int id = get_user_id(message.user.name);
    //     if (id == -1) {
    //         // what the heck!
    //         log_error("Unable to fetch user ID for name %s", message.user.name);
    //         return -1;
    //     }
    //     message.user.id = id;
    // }

    // insert into the database
    char from_id_buf[32];
    snprintf(from_id_buf, sizeof(from_id_buf), "%d", message.from.id);

    char to_id_buf[32];
    snprintf(to_id_buf, sizeof(to_id_buf), "%d", message.to.id);

    const char *paramValues[3] = {
        from_id_buf,
        to_id_buf,
        message.content
    };

    PGresult *res = PQexecParams(conn,
        "INSERT INTO messages (from_id, to_id, content) VALUES ($1, $2, $3) RETURNING id",
        3,
        NULL,
        paramValues,
        NULL,
        NULL,
        0
    );

    if (!res) {
        log_error("PQexecParams failed: %s", PQerrorMessage(conn));
        return -1;
    }

    if (PQntuples(res) > 0) {
        int id = atoi(PQgetvalue(res, 0, 0));
        PQclear(res);
        return id;
    }
    return -1;
}

message *_get_messages(PGresult *res, int *out_count) {
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        log_error("Failed to fetch messages: %s", PQerrorMessage(conn));
        if (res) PQclear(res);
        *out_count = 0;
        return NULL;
    }

    int count = PQntuples(res);
    message *messages = malloc(sizeof(message) * count);
    if (!messages) {
        log_error("Unable to allocate memory to get messages");
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        messages[i].id = atoi(PQgetvalue(res, i, 0));
        messages[i].from.name = strdup(PQgetvalue(res, i, 1));
        messages[i].to.name = strdup(PQgetvalue(res, i, 2));
        messages[i].from.id = atoi(PQgetvalue(res, i, 3));
        messages[i].to.id = atoi(PQgetvalue(res, i, 4));
        messages[i].content = strdup(PQgetvalue(res, i, 5));
        messages[i].read = (strcmp(PQgetvalue(res, i, 7), "t") == 0);

        // Parse timestamp
        char *ts_str = PQgetvalue(res, i, 6);
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        if (strptime(ts_str, "%Y-%m-%d %H:%M:%S", &tm)) {
            messages[i].time = mktime(&tm);
        } else {
            messages[i].time = 0; // fallback
        }
    }

    PQclear(res);
    *out_count = count;
    return messages;
}

message *get_messages(user user, bool unread_messages_only, int *out_count) {
    char *query =
        "SELECT m.id, "
        "uf.name, ut.name, "
        "m.from_id, m.to_id, "
        "m.content, m.timestamp, m.read "
        "FROM messages m "
        "JOIN users uf ON uf.id = m.from_id "
        "JOIN users ut ON ut.id = m.to_id "
        "WHERE m.to_id = $1 "
        "AND ($2 = false OR m.read = false) "
        "ORDER BY m.timestamp ASC";

    char user_id_buf[32];
    snprintf(user_id_buf, sizeof(user_id_buf), "%d", user.id);
    const char *paramValues[2] = { user_id_buf, unread_messages_only ? "true" : "false" };

    PGresult *res = PQexecParams(conn,
        query,
        2,
        NULL,
        paramValues,
        NULL,
        NULL,
        0
    );

    return _get_messages(res, out_count);
}

message *get_next(user user, bool _mark_read) {
    char *query =
        "SELECT m.id, "
        "uf.name, ut.name, "
        "m.from_id, m.to_id, "
        "m.content, m.timestamp, m.read "
        "FROM messages m "
        "JOIN users uf ON uf.id = m.from_id "
        "JOIN users ut ON ut.id = m.to_id "
        "WHERE m.to_id = $1 "
        "AND m.read = false "
        "ORDER BY m.timestamp ASC "
        "LIMIT 1";

    char user_id_buf[32];
    snprintf(user_id_buf, sizeof(user_id_buf), "%d", user.id);
    const char *paramValues[1] = { user_id_buf };

    PGresult *res = PQexecParams(conn,
        query,
        1,
        NULL,
        paramValues,
        NULL,
        NULL,
        0
    );

    int out_count;
    message *message = _get_messages(res, &out_count);
    // message should only be an array of 1

    if (_mark_read) {
        mark_read(message->id);
    }

    return message;
}

int mark_read(int message_id) {
    char *query = "UPDATE messages SET read = true WHERE id = $1";

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", message_id);
    const char *paramValues[1] = { buffer };

    PGresult *res = PQexecParams(conn,
        query,
        1,
        NULL,
        paramValues,
        NULL,
        NULL,
        0
    );

    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        log_error("Unable to mark message id %d as read: %s", message_id, PQerrorMessage(conn));
        if (res) PQclear(res);
        return 0;
    }

    PQclear(res);
    return 1;
}