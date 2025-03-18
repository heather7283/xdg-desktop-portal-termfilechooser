#include <sys/signalfd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>

#include "xdptf.h"
#include "filechooser.h"
#include "dbus.h"
#include "xmalloc.h"
#include "log.h"

static void print_usage_and_exit(FILE *stream, int retcode) {
    static const char usage[] =
        "Usage: xdg-desktop-portal-termfilechooser [options]\n"
        "\n"
        "    -c, --config        Path to config file\n"
        "    -r, --replace       Replace a running instance.\n"
        "    -l, --loglevel      Loglevel override.\n"
        "                        One of quiet, error, warn, info, debug.\n"
        "    -h, --help          Display this message and exit.\n"
        "\n";

    fputs(usage, stream);
    exit(retcode);
}

int dbus_event_handler(struct event_loop_item *item, uint32_t events) {
    struct sd_bus *bus = event_loop_item_get_data(item);

    log_print(DEBUG, "processing dbus events");
    int ret;
    if ((ret = sd_bus_process(bus, NULL)) < 0) {
        log_print(ERROR, "failed to process dbus events: %s", strerror(-ret));
        return ret;
    }

    return 0;
}

int sigint_sigterm_handler(struct event_loop_item *item, int signal) {
    log_print(INFO, "caught signal %d, exiting", signal);

    event_loop_quit(event_loop_item_get_loop(item), 0);

    return 0;
}

int sigchld_handler(struct event_loop_item *item, int signal) {
    struct xdptf *xdptf = event_loop_item_get_data(item);

    log_print(DEBUG, "caught SIGCHLD %d, running reaper", signal);

    pid_t pid;
    int wstatus;
    while ((pid = waitpid(-1, &wstatus, WNOHANG)) > 0) {
        if (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus)) {
            continue;
        }

        log_print(DEBUG, "child %d exited", pid);
        struct filechooser_request *request, *request_tmp;
        LIST_FOREACH_SAFE(request, &xdptf->requests, link, request_tmp) {
            if (request->picker_pid == pid) {
                log_print(DEBUG, "found request associated with child %d, finalizing it", pid);
                filechooser_request_finalize(request);
            }
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    int retcode = 0;

    struct xdptf xdptf = {0};

    static const char shortopts[] = "c:l:rh";
    static const struct option longopts[] = {
        { "config",      required_argument, NULL, 'c' },
        { "loglevel",    required_argument, NULL, 'l' },
        { "replace",     no_argument,       NULL, 'r' },
        { "help",        no_argument,       NULL, 'h' },
        { 0 }
    };

    bool replace = false;
    bool loglevel_override = false;
    enum log_loglevel loglevel_override_value;
    char *config_path = NULL;
    int c;
    while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) > 0) {
        switch (c) {
        case 'c':
            config_path = xstrdup(optarg);
            break;
        case 'l':
            loglevel_override = true;
            if (strcmp(optarg, "quiet") == 0) {
                loglevel_override_value = QUIET;
            } else if (strcmp(optarg, "error") == 0) {
                loglevel_override_value = ERROR;
            } else if (strcmp(optarg, "warn") == 0) {
                loglevel_override_value = WARN;
            } else if (strcmp(optarg, "info") == 0) {
                loglevel_override_value = INFO;
            } else if (strcmp(optarg, "debug") == 0) {
                loglevel_override_value = DEBUG;
            } else {
                print_usage_and_exit(stderr, 1);
            }
            break;
        case 'r':
            replace = true;
            break;
        case 'h':
            print_usage_and_exit(stdout, 0);
            break;
        default:
            print_usage_and_exit(stderr, 1);
            break;
        }
    }

    if (loglevel_override) {
        log_init(stderr, loglevel_override_value);
    } else {
        log_init(stderr, INFO);
    }

    if (config_init(&xdptf.config, config_path) < 0) {
        log_print(ERROR, "failed to parse config");
        retcode = 1;
        goto cleanup;
    }

    if (!loglevel_override) {
        log_print(INFO, "reinitialising logging with loglevel %d", xdptf.config.loglevel);
        log_init(stderr, xdptf.config.loglevel);
    }

    if (dbus_init(&xdptf, replace) < 0) {
        log_print(ERROR, "failed to initialise dbus");
        retcode = 1;
        goto cleanup;
    }

    xdptf.event_loop = event_loop_create();
    if (xdptf.event_loop == NULL) {
        log_print(ERROR, "failed to create event loop");
        retcode = 1;
        goto cleanup;
    }
    event_loop_add_pollable(xdptf.event_loop, xdptf.sd_bus_fd, EPOLLIN, false,
                            dbus_event_handler, xdptf.sd_bus);
    event_loop_add_signal(xdptf.event_loop, SIGINT, sigint_sigterm_handler, NULL);
    event_loop_add_signal(xdptf.event_loop, SIGTERM, sigint_sigterm_handler, NULL);
    event_loop_add_signal(xdptf.event_loop, SIGCHLD, sigchld_handler, &xdptf);

    retcode = event_loop_run(xdptf.event_loop);

cleanup: {} /* Label followed by a declaration is a C23 extension */
    struct filechooser_request *request, *request_tmp;
    LIST_FOREACH_SAFE(request, &xdptf.requests, link, request_tmp) {
        filechooser_request_cleanup(request);
    };

    dbus_cleanup(&xdptf);
    event_loop_cleanup(xdptf.event_loop);
    config_cleanup(&xdptf.config);
    free(config_path);

    return retcode;
}

