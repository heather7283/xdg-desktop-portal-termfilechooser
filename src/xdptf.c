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
#include "event_loop.h"
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

int dbus_event_handler(struct event_loop *loop, struct event_loop_item *item) {
    struct sd_bus *bus = item->data;

    log_print(DEBUG, "processing dbus events");
    int ret;
    if ((ret = sd_bus_process(bus, NULL)) < 0) {
        log_print(ERROR, "failed to process dbus events: %s", strerror(-ret));
        return ret;
    }

    return 0;
}

int signals_handler(struct event_loop *loop, struct event_loop_item *item) {
    log_print(DEBUG, "processing signals");
    struct signalfd_siginfo siginfo;
    if (read(item->fd, &siginfo, sizeof(siginfo)) != sizeof(siginfo)) {
        log_print(ERROR, "failed to read signalfd_siginfo from signalfd: %s", strerror(errno));
        return -1;
    }

    switch (siginfo.ssi_signo) {
    case SIGINT:
        log_print(INFO, "caught SIGINT, stopping main loop");
        event_loop_quit(loop);
        break;
    case SIGTERM:
        log_print(INFO, "caught SIGTERM, stopping main loop");
        event_loop_quit(loop);
        break;
    case SIGCHLD:
        pid_t pid;
        log_print(DEBUG, "caught SIGCHLD, running reaper");
        while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
            log_print(DEBUG, "reaped zombie with pid %d", pid);
        }
    }

    return 0;
}

int signalfd_init(void) {
    int fd;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        die("failed to block signals: %s", strerror(errno));
    }

    if ((fd = signalfd(-1, &mask, SFD_CLOEXEC)) < 0) {
        die("failed to set up signalfd: %s", strerror(errno));
    }

    return fd;
}

int main(int argc, char **argv) {
    int retcode = 0;
    int signal_fd = -1;

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
    }

    if (config_init(&xdptf.config, config_path) < 0) {
        log_print(ERROR, "failed to parse config");
        retcode = 1;
        goto cleanup;
    }

    if (!loglevel_override) {
        log_init(stderr, xdptf.config.loglevel);
    }

    dbus_init(&xdptf, replace);

    signal_fd = signalfd_init();

    event_loop_init(&xdptf.event_loop);
    event_loop_add_item(&xdptf.event_loop, xdptf.sd_bus_fd, dbus_event_handler, xdptf.sd_bus);
    event_loop_add_item(&xdptf.event_loop, signal_fd, signals_handler, NULL);

    event_loop_run(&xdptf.event_loop);

cleanup:
    filechooser_requests_cleanup();
    dbus_cleanup(&xdptf);
    event_loop_cleanup(&xdptf.event_loop);
    config_cleanup(&xdptf.config);
    if (config_path != NULL) {
        free(config_path);
    }

    return retcode;
}

