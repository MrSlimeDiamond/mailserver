#include "config.h"
#include <confuse.h>

cfg_t *config;

// TODO: Generate a config file
int config_init(void) {
    cfg_opt_t opts[] = {
        CFG_STR(CONFIG_ADDRESS, "0.0.0.0", CFGF_NONE),
        CFG_INT(CONFIG_PORT, 4000, CFGF_NONE),
        CFG_STR(CONFIG_DBUSER, "mailserver", CFGF_NONE),
        CFG_STR(CONFIG_DBNAME, "mailserver", CFGF_NONE),
        CFG_STR(CONFIG_DBPASSWORD, "yeet", CFGF_NONE),
        CFG_STR(CONFIG_DBHOST, "127.0.0.1", CFGF_NONE),
        CFG_INT(CONFIG_DBPORT, 5432, CFGF_NONE),
        CFG_END()
    };
    config = cfg_init(opts, CFGF_NONE);
    if (cfg_parse(config, "mailserver.conf") == CFG_PARSE_ERROR) {
        return 1;
    }

    // config is now defined

    return 0;
}