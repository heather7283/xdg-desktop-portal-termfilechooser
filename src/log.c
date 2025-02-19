#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

#include "log.h"

static struct log_config log_config = {
    .stream = NULL,
    .loglevel = INFO,
    .colors = false,
};

void log_init(FILE *stream, enum log_loglevel level) {
    log_config.stream = stream;
    log_config.loglevel = level;
    log_config.colors = isatty(fileno(stream));
}

void log_print(enum log_loglevel level, char *message, ...) {
    if (log_config.stream == NULL) {
        fprintf(stderr, "logger error: log() called before log_init()\n");
        abort();
    }

    if (level > log_config.loglevel) {
        return;
    }

    char level_char = '?';
    switch (level) {
    case ERROR:
        if (log_config.colors) {
            fprintf(log_config.stream, LOG_ANSI_COLORS_ERROR);
        }
        level_char = 'E';
        break;
    case WARN:
        if (log_config.colors) {
            fprintf(log_config.stream, LOG_ANSI_COLORS_WARN);
        }
        level_char = 'W';
        break;
    case INFO:
        /* no special color here... */
        level_char = 'I';
        break;
    case DEBUG:
        if (log_config.colors) {
            fprintf(log_config.stream, LOG_ANSI_COLORS_DEBUG);
        }
        level_char = 'D';
        break;
    case TRACE:
        if (log_config.colors) {
            fprintf(log_config.stream, LOG_ANSI_COLORS_DEBUG);
        }
        level_char = 'T';
        break;
    default:
        fprintf(stderr, "logger error: unknown loglevel %d\n", level);
        abort();
    }

    fprintf(log_config.stream, "%c ", level_char);

    va_list args;
    va_start(args, message);
    vfprintf(log_config.stream, message, args);
    va_end(args);

    if (log_config.colors) {
        fprintf(log_config.stream, LOG_ANSI_COLORS_RESET);
    }
    fprintf(log_config.stream, "\n");

    fflush(log_config.stream);
}

