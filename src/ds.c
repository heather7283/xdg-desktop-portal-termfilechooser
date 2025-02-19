#include <stdlib.h>
#include <string.h>

#include "ds.h"
#include "xmalloc.h"

/* initialise the dynamic string */
void ds_init(struct ds *ds) {
    ds->length = 0;
    ds->capacity = 0;
    ds->data = NULL;
}

/* append raw bytes to the dynamic string */
void ds_append_bytes(struct ds *ds, const void *data, size_t data_len) {
    size_t data_len_with_null = data_len + 1;
    /* check if realloc is needed */
    if (ds->length + data_len_with_null > ds->capacity) {
        /* try doubling the capacity first */
        ds->capacity = (ds->capacity == 0) ? data_len_with_null : (ds->capacity * 2);
        /* still not big enough? */
        if (ds->length + data_len_with_null > ds->capacity) {
            ds->capacity = ds->length + data_len_with_null;
        }
        ds->data = xrealloc(ds->data, ds->capacity);
    }

    memcpy(ds->data + ds->length, data, data_len);
    ds->length += data_len;

    /* ensure the string is null-terminated */
    ds->data[ds->length] = '\0';
}

/* free the dynamic string */
void ds_free(struct ds *ds) {
    if (ds->data != NULL) {
        free(ds->data);
    }
    ds->data = NULL;
    ds->length = 0;
    ds->capacity = 0;
}

