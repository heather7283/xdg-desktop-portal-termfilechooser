#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "log.h"
#include "xmalloc.h"

enum default_paths {
    XDG_CONFIG_HOME = 0,
    ETC_XDG = 1,
    END = 2,
};

/*
 *  1 - path constructed succesfully
 *  0 - failed to construct path
 * -1 - no more paths to check
 */
static int get_next_config_path(const char **ppath) {
    static enum default_paths current = XDG_CONFIG_HOME;
    static char path[PATH_MAX];

    switch (current++) {
    case XDG_CONFIG_HOME: {
        const char *xdg_config_home = getenv("XDG_CONFIG_HOME");
        if (xdg_config_home == NULL) {
            log_print(WARN, "config: XDG_CONFIG_HOME is unset");

            const char *home = getenv("HOME");
            if (home == NULL) {
                log_print(WARN, "config: HOME is unset");
                return 0;
            }

            snprintf(path, sizeof(path),
                     "%s/.config/xdg-desktop-portal-termfilechooser/config", home);
            *ppath = path;
            return 1;
        } else {
            snprintf(path, sizeof(path),
                     "%s/xdg-desktop-portal-termfilechooser/config", xdg_config_home);
            *ppath = path;
            return 1;
        }
    }
    case ETC_XDG:
        snprintf(path, sizeof(path), "/etc/xdg/xdg-desktop-portal-termfilechooser/config");
        *ppath = path;
        return 1;
    case END:
        return -1;
    default:
        die("config: get_next_config_path: illegal enum value: %d", current);
    }
}

static int config_parse_file(struct xdptf_config *config, const char *path) {
    int ret = 0;
    int line_number = 0;
    char *line = NULL;
    size_t buf_size;
    ssize_t line_len;

    log_print(INFO, "config: parsing config file %s", path);

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        log_print(ERROR, "config: failed to open %s: %s", path, strerror(errno));
        ret = -1;
        goto out;
    }

    while ((line_len = getline(&line, &buf_size, f)) > 0) {
        line_number += 1;

        /* empty line */
        if (line_len == 1) {
            log_print(DEBUG, "config: line %d: empty, skipping", line_number);
            continue;
        }

        /* comment */
        if (line[0] == '#') {
            log_print(DEBUG, "config: line %d: comment, skipping", line_number);
            continue;
        }

        /* strip newline */
        if (line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
        }

        char *k = line;
        char *v = NULL;
        /* find equals sign */
        for (char *p = k; *p != '\0'; p++) {
            if (*p == '=') {
                *p = '\0';
                v = p + 1;
                break;
            }
        }
        if (v == NULL) {
            log_print(WARN, "config: line %d: no '=' found, skipping", line_number);
            continue;
        }
        if (strlen(k) < 1) {
            log_print(WARN, "config: line %d: empty key, skipping", line_number);
            continue;
        }
        if (strlen(v) < 1) {
            log_print(WARN, "config: line %d: empty value, skipping", line_number);
            continue;
        }
        log_print(DEBUG, "config: line %d: key %s, value %s", line_number, k, v);

        if (strcmp(k, "picker_cmd") == 0) {
            config->picker_cmd = xstrdup(v);
        } else if (strcmp(k, "default_dir") == 0) {
            config->default_dir = xstrdup(v);
        } else if (strcmp(k, "loglevel") == 0) {
            if (strcmp(v, "quiet") == 0) {
                config->loglevel = QUIET;
            } else if (strcmp(v, "error") == 0) {
                config->loglevel = ERROR;
            } else if (strcmp(v, "warn") == 0) {
                config->loglevel = WARN;
            } else if (strcmp(v, "info") == 0) {
                config->loglevel = INFO;
            } else if (strcmp(v, "debug") == 0) {
                config->loglevel = DEBUG;
            } else {
                log_print(ERROR, "config: line %d: %s is not a valid loglevel", line_number, v);
                ret = -1;
                goto out;
            }
        } else {
            log_print(WARN, "config: line %d: %s is not a valid key", line_number, k);
        }
    }

out:
    if (line != NULL) {
        free(line);
    }
    if (f != NULL) {
        fclose(f);
    }

    return ret;
}

static int config_parse(struct xdptf_config *config, const char *path) {
    if (path != NULL) {
        return config_parse_file(config, path);
    }

    int ret;
    const char *default_path;
    while ((ret = get_next_config_path(&default_path)) >= 0) {
        if (ret == 0) {
            continue;
        }

        if (access(default_path, R_OK) < 0) {
            log_print(INFO, "config: skipping config file %s: %s", default_path, strerror(errno));
            continue;
        }

        return config_parse_file(config, default_path);
    }

    log_print(ERROR, "config: ran out of default config paths to check!");
    return -1;
}

static void config_fill_missing_values(struct xdptf_config *config) {
    if (config->default_dir == NULL) {
        const char *home = getenv("HOME");
        if (home != NULL) {
            config->default_dir = xstrdup(home);
        } else {
            config->default_dir = xstrdup("/tmp");
        }
        log_print(INFO, "config: default_dir not provided, using default: %s", config->default_dir);
    }
    if (config->picker_cmd == NULL) {
        /* TODO: instead of hardcoding /usr/share make meson pass the data path here */
        config->picker_cmd = xstrdup("/usr/share/xdg-desktop-portar-termfilechooser/lf-wrapper.sh");
        log_print(INFO, "config: picker_cmd not provided, using default: %s", config->picker_cmd);
    }
}

static int config_verify(struct xdptf_config *config) {
    if (access(config->picker_cmd, X_OK) < 0) {
        log_print(ERROR, "config: %s is not executable: %s", config->picker_cmd, strerror(errno));
        return -1;
    }

    struct stat sb;
    if (stat(config->default_dir, &sb) < 0 || !S_ISDIR(sb.st_mode)) {
        log_print(ERROR, "config: %s is not a directory", config->default_dir);
        return -1;
    }

    return 0;
}

int config_init(struct xdptf_config *config, const char *path) {
    if (config_parse(config, path) < 0) {
        return -1;
    }
    config_fill_missing_values(config);
    return config_verify(config);
}

void config_cleanup(struct xdptf_config *config) {
    if (config->default_dir != NULL) {
        free(config->default_dir);
    }
    if (config->picker_cmd != NULL) {
        free(config->picker_cmd);
    }
}

