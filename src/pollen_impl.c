#include <stdlib.h>
#include "xmalloc.h"
#define POLLEN_CALLOC(n, size) xcalloc(n, size)
#define POLLEN_FREE(ptr) free(ptr)

#include "log.h"
#define POLLEN_LOG_DEBUG(fmt, ...) log_print(DEBUG, "event loop: " fmt, ##__VA_ARGS__)
#define POLLEN_LOG_INFO(fmt, ...) log_print(INFO, "event loop: " fmt, ##__VA_ARGS__)
#define POLLEN_LOG_WARN(fmt, ...) log_print(WARN, "event loop: " fmt, ##__VA_ARGS__)
#define POLLEN_LOG_ERR(fmt, ...) log_print(ERROR, "event loop: " fmt, ##__VA_ARGS__)

#define POLLEN_IMPLEMENTATION
#include "thirdparty/pollen.h"
