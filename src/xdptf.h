#ifndef XDPTF_H
#define XDPTF_H

#include "config.h"
#include "thirdparty/pollen.h"
#include "thirdparty/queue.h"

struct xdptf {
    struct xdptf_config config;
    struct pollen_loop *event_loop;

    struct sd_bus *sd_bus;
    int sd_bus_fd;
    struct sd_bus_slot *filechooser_vtable_slot;
    struct sd_bus_slot *name_owner_changed_slot;

    LIST_HEAD(requests, filechooser_request) requests;
};

#endif

