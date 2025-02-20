#ifndef CONFIG_H
#define CONFIG_H

#include "log.h"

struct xdptf_config {
    char *picker_cmd;
    char *default_dir;
    enum log_loglevel loglevel;
    bool replace;
};

/* if path is not NULL it will ignore default locations and try to parse file at path */
int config_init(struct xdptf_config *config, const char *path);
void config_cleanup(struct xdptf_config *config);

#endif /* #ifndef CONFIG_H */

