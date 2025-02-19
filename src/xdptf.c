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
        "    -p, --picker        Picker executable\n"
        "    -d, --default-dir   Default save dir\n"
        "    -r, --replace       Replace a running instance.\n"
        "    -h, --help          Display this message and exit.\n"
        "\n";

    fputs(usage, stream);
    exit(retcode);
}

static int parse_command_line(int *argc, char ***argv, struct xdptf_config *config) {
    static const char shortopts[] = "p:d:l:rh";
    static const struct option longopts[] = {
        { "picker",      required_argument, NULL, 'p' },
        { "default-dir", required_argument, NULL, 'd' },
        { "replace",     no_argument,       NULL, 'r' },
        { "loglevel",    required_argument, NULL, 'l' },
        { "help",        no_argument,       NULL, 'h' },
        { 0 }
    };

    int c;
    while ((c = getopt_long(*argc, *argv, shortopts, longopts, NULL)) > 0) {
        switch (c) {
        case 'p':
            /* TODO: test if cmd is executable */
            config->picker_cmd = xstrdup(optarg);
            break;
        case 'd':
            /* TODO: test if dir exists */
            config->default_dir = xstrdup(optarg);
            break;
        case 'r':
            config->replace = true;
            break;
        case 'l':
            /* TODO: this is stupid, make it not stupid */
            errno = 0;
            config->loglevel = strtoul(optarg, NULL, 10);
            if (errno != 0) {
                log_print(ERROR, "failed to convert %s to number: %s",
                          optarg, strerror(errno));
                return -1;
            }
            break;
        case 'h':
            print_usage_and_exit(stdout, 0);
            break;
        default:
            print_usage_and_exit(stderr, 1);
            break;
        }
    }

    return 0;
}

static void config_init(struct xdptf_config *config) {
    config->picker_cmd = NULL;
    config->default_dir = NULL;
    config->replace = false;
}

static void config_cleanup(struct xdptf_config *config) {
    if (config->picker_cmd != NULL) {
        free(config->picker_cmd);
    }
    if (config->default_dir != NULL) {
        free(config->default_dir);
    }
}

int dbus_event_handler(struct event_loop *loop, struct event_loop_item *item) {
    struct sd_bus *bus = item->data;

    log_print(TRACE, "processing dbus events");
    int ret;
    if ((ret = sd_bus_process(bus, NULL)) < 0) {
        log_print(ERROR, "failed to process dbus events: %s", strerror(-ret));
        return ret;
    }

    return 0;
}

int signals_handler(struct event_loop *loop, struct event_loop_item *item) {
    log_print(TRACE, "processing signals");
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

    log_init(stderr, ERROR);

    config_init(&xdptf.config);
    if (parse_command_line(&argc, &argv, &xdptf.config) < 0) {
        log_print(ERROR, "error while parsing command line");
        retcode = 1;
        goto cleanup;
    }

    log_init(stderr, xdptf.config.loglevel);

    dbus_init(&xdptf);

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

    return retcode;
}

