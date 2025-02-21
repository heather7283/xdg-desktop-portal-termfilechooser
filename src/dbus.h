#ifndef DBUS_H
#define DBUS_H

#include "xdptf.h"

int dbus_init(struct xdptf *xdptf, bool replace);
void dbus_cleanup(struct xdptf *xdptf);

#endif /* #ifndef DBUS_H */

