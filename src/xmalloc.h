#ifndef XMALLOC_H
#define XMALLOC_H

#include <stddef.h>

void *xmalloc(size_t size);
void *xcalloc(size_t n, size_t size);
void *xrealloc(void *ptr, size_t size);

char *xstrdup(const char *s);

#endif /* #ifndef XMALLOC_H */

