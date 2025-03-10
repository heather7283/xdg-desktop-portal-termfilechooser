#include "thirdparty/event_loop.h"
#include "xdptf.h"
#include "filechooser.h"
#include "log.h"

static const sd_bus_vtable filechooser_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("OpenFile", "osssa{sv}", "ua{sv}", method_open_file, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SaveFile", "osssa{sv}", "ua{sv}", method_save_file, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static int handle_name_lost(sd_bus_message *msg, void *data, sd_bus_error *ret_error) {
    struct xdptf *xdptf = data;

    log_print(INFO, "dbus: lost name, closing connection");

    event_loop_quit(xdptf->event_loop, 0);
    return 1;
}

int dbus_init(struct xdptf *xdptf, bool replace) {
    static const char service_name[] = "org.freedesktop.impl.portal.desktop.termfilechooser";
    static const char object_path[] = "/org/freedesktop/portal/desktop";

    int ret = 0;

    if ((ret = sd_bus_open_user(&xdptf->sd_bus)) < 0) {
        log_print(ERROR, "dbus: failed to connect to user bus: %s", strerror(-ret));
        return ret;
    }
    log_print(INFO, "connected to dbus");

    if ((ret = xdptf->sd_bus_fd = sd_bus_get_fd(xdptf->sd_bus)) < 0) {
        log_print(ERROR, "dbus: failed to get dbus fd: %s", strerror(-ret));
        return ret;
    }

    static const char filechooser_interface_name[] = "org.freedesktop.impl.portal.FileChooser";
    log_print(DEBUG, "dbus: init %s", filechooser_interface_name);
    ret = sd_bus_add_object_vtable(xdptf->sd_bus, &xdptf->filechooser_vtable_slot,
                                   object_path, filechooser_interface_name,
                                   filechooser_vtable, xdptf);
    if (ret < 0) {
        log_print(ERROR, "failed to add filechooser vtable: %s", strerror(-ret));
        return ret;
    }

    uint64_t flags = SD_BUS_NAME_ALLOW_REPLACEMENT;
    if (replace) {
        flags |= SD_BUS_NAME_REPLACE_EXISTING;
    }
    if ((ret = sd_bus_request_name(xdptf->sd_bus, service_name, flags)) < 0) {
        log_print(ERROR, "dbus: failed to acquire service name: %s", strerror(-ret));
        return ret;
    }

    const char *unique_name;
    if ((ret = sd_bus_get_unique_name(xdptf->sd_bus, &unique_name)) < 0) {
        log_print(ERROR, "dbus: failed to get unique bus name: %s", strerror(-ret));
        return ret;
    }
    log_print(INFO, "got unique name: %s", unique_name);

    static char match[1024];
    snprintf(match, sizeof(match),
             "sender='org.freedesktop.DBus',"
             "type='signal',"
             "interface='org.freedesktop.DBus',"
             "member='NameOwnerChanged',"
             "path='/org/freedesktop/DBus',"
             "arg0='%s',"
             "arg1='%s'",
             service_name, unique_name);

    if ((ret = sd_bus_add_match(xdptf->sd_bus, &xdptf->name_owner_changed_slot,
                                match, handle_name_lost, xdptf)) < 0) {
        log_print(ERROR, "dbus: failed to add NameOwnerChanged signal match: %s", strerror(-ret));
        return ret;
    }

    return 0;
}

void dbus_cleanup(struct xdptf *xdptf) {
    if (xdptf->name_owner_changed_slot != NULL) {
        sd_bus_slot_unref(xdptf->name_owner_changed_slot);
        xdptf->name_owner_changed_slot = NULL;
    }

    if (xdptf->filechooser_vtable_slot != NULL) {
        sd_bus_slot_unref(xdptf->filechooser_vtable_slot);
        xdptf->filechooser_vtable_slot = NULL;
    }

    if (xdptf->sd_bus != NULL) {
        sd_bus_flush(xdptf->sd_bus);
        sd_bus_close(xdptf->sd_bus);
        sd_bus_unref(xdptf->sd_bus);
    }
}
