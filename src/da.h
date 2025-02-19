#ifndef DA_H
#define DA_H

#include <stddef.h>

/* dynamic array of chars */
struct da {
    size_t length;
    size_t capacity;
    char *data;
};

void da_init(struct da *da);
void da_append(struct da *da, const void *data, size_t data_len);
void da_free(struct da *da);

#endif /* #ifndef DA_H */

