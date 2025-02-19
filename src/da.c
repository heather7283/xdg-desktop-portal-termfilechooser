#include <stdlib.h>
#include <string.h>

#include "da.h"
#include "xmalloc.h"

/* initialise the dynamic array */
void da_init(struct da *da) {
    da->length = 0;
    da->capacity = 0;
    da->data = NULL;
}

/* append data to the dynamic array */
void da_append(struct da *da, const void *data, size_t data_len) {
    /* check if resize is needed */
    if (da->length + data_len > da->capacity) {
        /* double the capacity or set it to data_len if it's the first allocation */
        da->capacity = (da->capacity == 0) ? data_len : da->capacity * 2;
        if (da->length + data_len > da->capacity) {
            da->capacity = da->length + data_len;
        }
        da->data = xrealloc(da->data, da->capacity);
    }

    memcpy(da->data + da->length, data, data_len);
    da->length += data_len;
}

/* free the dynamic array */
void da_free(struct da *da) {
    if (da->data != NULL) {
        free(da->data);
    }
    da->data = NULL;
    da->length = 0;
    da->capacity = 0;
}

