#pragma once

#include <confuse.h>

#define CONFIG_ADDRESS "address"
#define CONFIG_PORT "port"
#define CONFIG_DBUSER "dbuser"
#define CONFIG_DBNAME "dbname"
#define CONFIG_DBPASSWORD "dbpassword"
#define CONFIG_DBHOST "dbhost"
#define CONFIG_DBPORT "dbport"

extern cfg_t *config;

int config_init(void);