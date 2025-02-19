#ifndef XDPTF_H
#define XDPTF_H

#include <stdbool.h>

#include "event_loop.h"
#include "log.h"

struct xdptf_config {
    char *picker_cmd;
    char *default_dir;
    enum log_loglevel loglevel;
    bool replace;
};

struct xdptf {
    struct xdptf_config config;
    struct event_loop event_loop;

    struct sd_bus *sd_bus;
    int sd_bus_fd;
    struct sd_bus_slot *filechooser_vtable_slot;
    struct sd_bus_slot *name_owner_changed_slot;
};

#endif

