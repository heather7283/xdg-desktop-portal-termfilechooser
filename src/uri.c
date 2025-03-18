#include <string.h>
#include <stdbool.h>

#include "uri.h"
#include "xmalloc.h"

/*
 * If you stumbled upon this code, try putting this function into godbolt
 * with gcc and look at assembly it generates. Isn't it crazy? I think
 * it's pretty crazy. Compilers are so insanely fucking smart nowadays.
 */
static inline bool needs_encoding(char c) {
    /* / is technically a reserved character but it's allowed in path uris */
    return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' || c == '/');
}

/* returns a malloc'd string */
static char *uri_encode(const char *str) {
    /* several war crimes against programming have been commited */
    static const char prefix[] = "file://";

    /* prefix size + string size * 3 (%XX) + null terminator */
    size_t buf_len = strlen(prefix) + (strlen(str) * 3) + 1;
    char *uri = xmalloc(buf_len);

    const char *src;
    char *dst = stpcpy(uri, prefix);
    unsigned char c;
    for (src = str; (c = *src) != '\0'; src++) {
        if (needs_encoding(c)) {
            unsigned char low = (c & 0x0F);
            unsigned char high = (c >> 4);
            *dst++ = '%';
            *dst++ = (high < 10) ? (high + '0') : (high + 'A' - 10);
            *dst++ = (low < 10) ? (low + '0') : (low + 'A' - 10);
        } else {
            *dst++ = c;
        }
    }
    *dst = '\0';

    return uri;
}

/*
 * puts null-terminated malloc'd array of malloc'd strings in res.
 * returns number of items in array without null terminator.
 * if number of items is 0, do not free the array!
 */
int get_uris_from_string(char *str, char ***res) {
    if (str == NULL) {
        *res = NULL;
        return 0;
    }

    char **uris;

    /*
     * last line can not end with newline, I don't handle this case here and
     * assume that every lines ends with newline (and there are no empty ones)
     * also this algo is SLOW AS BALLS and I hate it
     */
    int n_lines = 0;
    for (char *p = str; *p != '\0'; p++) {
        if (*p == '\n') {
            n_lines += 1;
        }
    }

    if (n_lines == 0) {
        return 0;
    }

    /* +1 for null terminator at the end */
    uris = xmalloc((n_lines + 1) * sizeof(char *));

    int i = 0;
    char *line_start = str;
    for (char *p = line_start; *p != '\0'; p++) {
        if (*p == '\n') {
            *p = '\0'; /* replace newline with null terminator */

            char *uri;
            uri = uri_encode(line_start);

            uris[i++] = uri;
            line_start = p + 1;
        }
    }
    uris[i] = NULL;

    *res = uris;

    return n_lines;
}

