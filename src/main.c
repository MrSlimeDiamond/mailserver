#include <stdio.h>
#include "server.h"
#include "logging.h"
#include "config.h"

int main() {
    log_info("Starting mailserver");

    config_init();

    char *address = cfg_getstr(config, CONFIG_ADDRESS);
    int port = cfg_getint(config, CONFIG_PORT);

    server_start(address, port);
    server_stop();

    return 0;
}