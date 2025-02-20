#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "event_loop.h"
#include "xmalloc.h"
#include "log.h"

#define EPOLL_MAX_EVENTS 16

static bool fd_is_valid(int fd) {
    if ((fcntl(fd, F_GETFD) < 0) && (errno == EBADF)) {
        return false;
    }
    return true;
}

void event_loop_init(struct event_loop *loop) {
    log_print(DEBUG, "event loop: init");

    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) {
        die("event loop: failed to create epoll");
    }
}

void event_loop_cleanup(struct event_loop *loop) {
    log_print(DEBUG, "event loop: cleanup");

    struct event_loop_item *item, *item_tmp;
    LIST_FOREACH_SAFE(item, &loop->items, link, item_tmp) {
        event_loop_remove_item(loop, item);
    }

    close(loop->epoll_fd);
}

void event_loop_add_item(struct event_loop *loop, int fd,
                         event_loop_callback callback, void *data) {
    log_print(DEBUG, "event loop: adding fd %d to event loop", fd);

    struct event_loop_item *new_item = xmalloc(sizeof(*new_item));
    new_item->fd = fd;
    new_item->callback = callback;
    new_item->data = data;

    struct epoll_event epoll_event;
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = fd;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &epoll_event) < 0) {
        die("event loop: failed to add fd %d to epoll (%s)", fd, strerror(errno));
    }

    LIST_INSERT_HEAD(&loop->items, new_item, link);
}

void event_loop_remove_item(struct event_loop *loop, struct event_loop_item *item) {
    log_print(DEBUG, "event loop: removing fd %d from event loop", item->fd);

    LIST_REMOVE(item, link);

    if (fd_is_valid(item->fd)) {
        if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, item->fd, NULL) < 0) {
            die("event loop: failed to remove fd %d from epoll (%s)", item->fd, strerror(errno));
        }
        close(item->fd);
    } else {
        /* looking at you, dbus */
        log_print(WARN, "event loop: fd %d is not valid, was it closed somewhere else?", item->fd);
    }

    free(item);
}

void event_loop_run(struct event_loop *loop) {
    log_print(DEBUG, "event loop: run");

    int number_fds = -1;
    struct epoll_event events[EPOLL_MAX_EVENTS];

    loop->running = true;
    while (loop->running) {
        do {
            number_fds = epoll_wait(loop->epoll_fd, events, EPOLL_MAX_EVENTS, -1);
        } while (number_fds == -1 && errno == EINTR); /* epoll_wait failing with EINTR is normal */

        if (number_fds == -1) {
            die("event loop: epoll_wait error (%s)", strerror(errno));
        }

        log_print(DEBUG, "event loop: received events on %d fds", number_fds);

        for (int n = 0; n < number_fds; n++) {
            bool match_found = false;
            struct event_loop_item *item, *item_tmp;

            log_print(DEBUG, "event loop: processing fd %d", events[n].data.fd);

            LIST_FOREACH_SAFE(item, &loop->items, link, item_tmp) {
                if (item->fd == events[n].data.fd) {
                    match_found = true;
                    int ret = item->callback(loop, item);
                    if (ret < 0) {
                        log_print(ERROR, "event loop: callback returned non-zero, quitting");
                        loop->running = false;
                        goto out;
                    }
                };
            }

            if (!match_found) {
                die("event loop: no handlers were found for fd %d", events[n].data.fd);
            }
        }
    }

out:
    return;
}

void event_loop_quit(struct event_loop *loop) {
    log_print(DEBUG, "event loop: quit");

    loop->running = false;
}

