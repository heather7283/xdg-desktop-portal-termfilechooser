#ifndef DA_H
#define DA_H

#include <stddef.h>

/* dynamic string */
struct ds {
    /* length WITHOUT NULL TERMINATOR */
    size_t length;
    /* capacity WITH NULL TERMINATOR */
    size_t capacity;
    /* buffer, always null-terminated */
    char *data;
};

void ds_init(struct ds *ds);
void ds_append_bytes(struct ds *ds, const void *data, size_t data_len);
void ds_free(struct ds *ds);

#endif /* #ifndef DA_H */

