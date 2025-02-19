#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <stdbool.h>

#include "thirdparty/queue.h"

struct event_loop;
struct event_loop_item;

typedef int (*event_loop_callback)(struct event_loop *loop, struct event_loop_item *item);

struct event_loop_item {
    int fd;
    void *data;
    event_loop_callback callback;

    LIST_ENTRY(event_loop_item) link;
};

struct event_loop {
    bool running;
    int epoll_fd;

    LIST_HEAD(event_loop_items, event_loop_item) items;
};

void event_loop_init(struct event_loop *loop);
void event_loop_cleanup(struct event_loop *loop);
void event_loop_add_item(struct event_loop *loop, int fd, event_loop_callback callback, void *data);
void event_loop_remove_item(struct event_loop *loop, struct event_loop_item *item);
void event_loop_run(struct event_loop *loop);
void event_loop_quit(struct event_loop *loop);

#endif /* #ifndef EVENT_LOOP_H */

