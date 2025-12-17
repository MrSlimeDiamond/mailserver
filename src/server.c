#include "server.h"
#include "logging.h"
#include "handlers.h"
#include "storage.h"
#include "config.h"
#include <json.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#define QUEUE_CONNECTION 100
#define BUFFER_SIZE 65356
#define THREAD_STACK_SIZE 524288

int *connections = NULL;
int connection_count = 0;
size_t connection_capacity;
time_t start_time;
static char *invalid_request_response = "{\"status\":400,\"message\":\"Invalid request\"}\n";

void add_connection(int conn_id) {
    if (connection_count >= connection_capacity) {
        connection_capacity = (connection_capacity == 0) ? 4 : connection_capacity * 2;
        connections = realloc(connections, connection_capacity * sizeof(int));
    }
    connections[connection_count++] = conn_id;
}

void remove_connection(int conn_id) {
    for (size_t i = 0; i < connection_count; i++) {
        if (connections[i] == conn_id) {
            connections[i] = connections[connection_count - 1];
            connection_count--;
            return;
        }
    }
}

void to_lower_case(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

void send_invalid_request(int conn_id) {
    send(conn_id, invalid_request_response, strlen(invalid_request_response), 0);
}

void *connection_handler(void *sock_fd) {
    int conn_id = *((int *)sock_fd);
    free(sock_fd);

    char buffer[BUFFER_SIZE] = {0};

    while (recv(conn_id, buffer, BUFFER_SIZE, 0) > 0) {
        // log_debug("Received from conn_id=%d: %s", conn_id, buffer);

        // TODO: Skip on blank string input

        // interpret json from the buffer
        json_object *root = json_tokener_parse(buffer);

        memset(buffer, 0, BUFFER_SIZE);

        if (root == 0) {
            log_warn("Invalid packet received from conn_id=%d", conn_id);
            send_invalid_request(conn_id);
            continue;
        }

        // char *str = json_object_to_json_string(root);
        // log_debug("%s", str);

        json_object *type_object = json_object_object_get(root, "request_type");
        if (!json_object_is_type(type_object, json_type_string)) {
            send_invalid_request(conn_id);
            continue;
        }
        char *type = json_object_get_string(type_object);

        if (type == 0) {
            log_warn("Invalid packet received from conn_id=%d (no request_type given)", conn_id);
            send_invalid_request(conn_id);
            continue;
        }

        log_debug("Request type: %s", type);
        to_lower_case(type);

        if (strcmp(type, "status") == 0)         handle_status_request(conn_id);
        if (strcmp(type, "user_id") == 0)        handle_user_id_request(conn_id, root);
        if (strcmp(type, "submit_message") == 0) handle_submit_message_request(conn_id, root);
        if (strcmp(type, "get_messages") == 0)   handle_get_messages_request(conn_id, root);
        if (strcmp(type, "get_next") == 0)       handle_get_next_request(conn_id, root);
        if (strcmp(type, "mark_read") == 0)      handle_set_read_request(conn_id, root);
        if (strcmp(type, "close") == 0)          break;
    }

    log_debug("Closing connection conn_id=%d", conn_id);
    close(conn_id);
    remove_connection(conn_id);
    pthread_exit(NULL);
}

void server_start(char *address, int port) {
    log_info("Starting server on port %d", port);
    start_time = time(NULL);

    // open the database
    storage_open();

    pthread_t thread_id;
    pthread_attr_t attr;

    if (pthread_attr_init(&attr) != 0) {
        log_error("Unable to start thread: %s", strerror(errno));
        server_stop();
        exit(EXIT_FAILURE);
    }

    if (pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE) != 0) {
        log_error("Unable to set thread stack size: %s", strerror(errno));
        server_stop();
        exit(EXIT_FAILURE);
    }

    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        log_error("Unable to set thread detach state: %s", strerror(errno));
        server_stop();
        exit(EXIT_FAILURE);
    }

    int master_socket, conn_id;
    struct sockaddr_in server, client;
    signal(SIGPIPE, SIG_IGN);

    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        log_error("Unable to create socket!");
        server_stop();
        exit(EXIT_FAILURE);
    }
    log_info("Created socket");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(address);
    server.sin_port = htons(port);

    socklen_t addrlen = sizeof(struct sockaddr_in);

    if (bind(master_socket, (struct sockaddr*)&server, sizeof(server)) == -1) {
        log_error("Failed to bind to port: %s", strerror(errno));
        server_stop();
        exit(EXIT_FAILURE);
    }
    
    if (listen(master_socket, QUEUE_CONNECTION) == -1) {
        log_error("Unable to listen: %s", strerror(errno));
        server_stop();
        exit(EXIT_FAILURE);
    }

    while (1) {
        conn_id = accept(master_socket, (struct sockaddr*)&client, (socklen_t*)&addrlen);

        if (conn_id == -1) {
            log_warn("Unable to accept new connection");
            continue;
        }
        int *conn_id_heap = (int*)malloc(sizeof(int));

        if (conn_id_heap == NULL) {
            log_error("Unable to allocate memory for a connection id!");
            close(conn_id);
            continue;
        }

        *conn_id_heap = conn_id; // save it to the heap

        log_debug("Connection accepted from %s:%d", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        add_connection(conn_id);

        if (pthread_create(&thread_id, &attr, connection_handler, (void*)conn_id_heap) == -1) {
            log_error("Unable to create new thread for a connection!");
            close(conn_id);
            free(conn_id_heap);
            continue;
        }
    }
    free(connections);
}

void send_to_client(int conn_id, json_object* json) {
    char *buffer = json_object_to_json_string(json);
    int length = strlen(buffer);
    buffer[length] = '\n';
    buffer[length + 1] = '\0';

    send(conn_id, buffer, strlen(buffer), 0);
}

void broadcast_json(json_object *json) {
    for (size_t i = 0; i < connection_count; i++) {
        int conn_id = connections[i];
        send_to_client(conn_id, json);
    }
}

void server_stop(void) {
    log_info("Stopping!");
    cfg_free(config);
    storage_close();
}