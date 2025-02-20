#ifndef LOG_H
#define LOG_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define LOG_ANSI_COLORS_ERROR "\033[31m"
#define LOG_ANSI_COLORS_WARN  "\033[33m"
#define LOG_ANSI_COLORS_DEBUG "\033[2m"
#define LOG_ANSI_COLORS_RESET "\033[0m"

enum log_loglevel {
    QUIET,
    ERROR,
    WARN,
    INFO,
    DEBUG,
};

struct log_config {
    FILE *stream;
    enum log_loglevel loglevel;
    bool colors;
};

void log_init(FILE *stream, enum log_loglevel level);
void log_print(enum log_loglevel level, char *msg, ...);

#define die(msg, ...) \
    do { \
        log_print(ERROR, msg, ##__VA_ARGS__); \
        abort(); \
    } while(0)

#endif /* ifndef LOG_H */

