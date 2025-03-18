#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "filechooser.h"
#include "xdptf.h"
#include "log.h"
#include "xmalloc.h"
#include "picker.h"
#include "uri.h"

enum {
    PORTAL_RESPONSE_SUCCESS = 0,
    PORTAL_RESPONSE_CANCELLED = 1,
    PORTAL_RESPONSE_ENDED = 2
};

static const char interface_name[] = "org.freedesktop.impl.portal.Request";

static int method_close(sd_bus_message *msg, void *data, sd_bus_error *ret_error) {
    struct filechooser_request *request = data;
    int ret = 0;
    log_print(DEBUG, "request closed");

    if (kill(-request->picker_pid, SIGTERM) < 0) {
        log_print(WARN, "failed to kill picker: %s", strerror(errno));
    };

    sd_bus_message *reply = NULL;
    if ((ret = sd_bus_message_new_method_return(msg, &reply)) < 0) {
        log_print(ERROR, "sd_bus_message_new_method_return() failed: %s", strerror(-ret));
        return ret;
    }
    if ((ret = sd_bus_send(NULL, reply, NULL)) < 0) {
        log_print(ERROR, "sd_bus_send() failed: %s", strerror(-ret));
        return ret;
    }
    sd_bus_message_unref(reply);

    filechooser_request_cleanup(request);

    return 0;
}

static const sd_bus_vtable request_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Close", "", "", method_close, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END
};

static int send_response_error(struct filechooser_request *request) {
    int ret = 0;
    struct sd_bus_message *reply = request->response.message;

    if ((ret = sd_bus_message_append(reply, "u", PORTAL_RESPONSE_ENDED, 1)) < 0) {
        log_print(ERROR, "sd_bus_message_append() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_open_container(reply, 'a', "{sv}")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_send(NULL, reply, NULL)) < 0) {
        log_print(ERROR, "sd_bus_send() failed: %s", strerror(-ret));
        goto out;
    }
out:
    return ret;
}

static int send_response_cancelled(struct filechooser_request *request) {
    int ret = 0;
    struct sd_bus_message *reply = request->response.message;

    if ((ret = sd_bus_message_append(reply, "u", PORTAL_RESPONSE_CANCELLED, 1)) < 0) {
        log_print(ERROR, "sd_bus_message_append() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_open_container(reply, 'a', "{sv}")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_send(NULL, reply, NULL)) < 0) {
        log_print(ERROR, "sd_bus_send() failed: %s", strerror(-ret));
        goto out;
    }
out:
    return ret;
}

static int send_response_success(struct filechooser_request *request) {
    int ret = 0;
    struct sd_bus_message *reply = request->response.message;

    if ((ret = sd_bus_message_append(reply, "u", PORTAL_RESPONSE_SUCCESS, 1)) < 0) {
        log_print(ERROR, "sd_bus_message_append() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_open_container(reply, 'a', "{sv}")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_open_container(reply, 'e', "sv")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_append_basic(reply, 's', "uris")) < 0) {
        log_print(ERROR, "sd_bus_message_append_basic() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_open_container(reply, 'v', "as")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_append_strv(reply, request->response.uris)) < 0) {
        log_print(ERROR, "sd_bus_message_append_strv() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto out;
    }
    if ((ret = sd_bus_send(NULL, reply, NULL)) < 0) {
        log_print(ERROR, "sd_bus_send() failed: %s", strerror(-ret));
        goto out;
    }
out:
    return ret;
}

int filechooser_request_finalize(struct filechooser_request *request) {
    /* TODO: check number of uris returned when only one uri is needed */
    char **uris;
    int n_uris = get_uris_from_string(request->buffer.data, &uris);

    log_print(DEBUG, "got %d uris", n_uris);

    int ret;
    if (n_uris == 0) {
        ret = send_response_cancelled(request);
    } else {
        request->response.n_uris = n_uris;
        request->response.uris = uris;
        ret = send_response_success(request);
    }

    filechooser_request_cleanup(request);

    return ret;
}

static int request_fd_event_handler(struct event_loop_item *item, uint32_t events) {
    struct filechooser_request *request = event_loop_item_get_data(item);

    static char buf[4096];
    ssize_t bytes_read;
    while (true) {
        bytes_read = read(request->pipe_fd, buf, sizeof(buf));
        if (bytes_read > 0) {
            ds_append_bytes(&request->buffer, buf, bytes_read);
        } else if (bytes_read == 0) {
            /* EOF */
            log_print(DEBUG, "EOF on pipe fd %d", request->pipe_fd);
            return filechooser_request_finalize(request);
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* no more data to read */
            return 0;
        } else {
            log_print(ERROR, "failed to read from pipe (fd %d): %s",
                      request->pipe_fd, strerror(errno));
            send_response_error(request);
            filechooser_request_cleanup(request);
            return -1;
        }
    }
}

int method_save_file(sd_bus_message *msg, void *data, sd_bus_error *ret_error) {
    struct xdptf *xdptf = data;

    int ret = 0;

    char *handle, *app_id, *parent_window, *title;
    char *current_folder = NULL;
    char *current_name = NULL;

    log_print(DEBUG, "save_file_handler: fired");

    if ((ret = sd_bus_message_read(msg, "osss", &handle, &app_id, &parent_window, &title)) < 0) {
        log_print(ERROR, "save_file_handler: sd_bus_message_read() failed");
        goto err;
    };
    log_print(DEBUG, "save_file_handler: handle = %s", handle);
    log_print(DEBUG, "save_file_handler: app_id = %s", app_id);
    log_print(DEBUG, "save_file_handler: parent_window = %s", parent_window);
    log_print(DEBUG, "save_file_handler: title = %s", title);

    if ((ret = sd_bus_message_enter_container(msg, 'a', "{sv}")) < 0) {
        log_print(ERROR, "save_file_handler: sd_bus_message_enter_container() failed");
        goto err;
    }
    while (sd_bus_message_enter_container(msg, 'e', "sv") > 0) {
        char *key;

        if ((ret = sd_bus_message_read(msg, "s", &key)) < 0) {
            log_print(ERROR, "save_file_handler: sd_bus_message_read() failed");
            goto err;
        }

        /* TODO: also get current_file */
        if (strcmp(key, "current_name") == 0) {
            sd_bus_message_read(msg, "v", "s", &current_name);
            log_print(DEBUG, "save_file_handler: option current_name = %s", current_name);
        } else if (strcmp(key, "current_folder") == 0) {
            const void *ptr = NULL;
            size_t size = 0;
            if ((ret = sd_bus_message_enter_container(msg, 'v', "ay")) < 0) {
                log_print(ERROR, "save_file_handler: sd_bus_message_enter_container() failed");
                goto err;
            }
            if ((ret = sd_bus_message_read_array(msg, 'y', &ptr, &size)) < 0) {
                log_print(ERROR, "save_file_handler: sd_bus_message_read_array() failed");
                goto err;
            }
            current_folder = (char *)ptr;
            log_print(DEBUG, "save_file_handler: option current_folder = %s", current_folder);
        } else {
            log_print(DEBUG, "save_file_handler: option %s IGNORED", key);
            if ((ret = sd_bus_message_skip(msg, "v")) < 0) {
                log_print(ERROR, "save_file_handler: sd_bus_message_skip() failed");
                goto err;
            }
        }

        if ((ret = sd_bus_message_exit_container(msg)) < 0) {
            log_print(ERROR, "save_file_handler: sd_bus_message_exit_container() failed");
            goto err;
        }
    }

    sd_bus_message *response;
    if ((ret = sd_bus_message_new_method_return(msg, &response)) < 0) {
        log_print(ERROR, "sd_bus_message_new_method_return() failed: %s", strerror(-ret));
        goto err;
    }

    struct save_file_request_data request_data = {
        .current_folder = current_folder,
        .current_name = current_name,
    };
    pid_t child_pid;
    ret = exec_picker(xdptf->config.picker_cmd, SAVE_FILE, &request_data, &child_pid);
    if (ret < 0) {
        log_print(ERROR, "exec_picker() failed: %s", strerror(-ret));
        goto err;
    }
    int pipe_fd = ret;

    struct filechooser_request *new_request = xcalloc(1, sizeof(*new_request));
    ds_init(&new_request->buffer);
    new_request->type = SAVE_FILE;
    new_request->response.message = response;
    new_request->pipe_fd = pipe_fd;
    new_request->picker_pid = child_pid;

    if ((ret = sd_bus_add_object_vtable(sd_bus_message_get_bus(msg), &new_request->slot, handle,
                                        interface_name, request_vtable, new_request)) < 0) {
        log_print(ERROR, "sd_bus_add_object_vtable() failed: %s", strerror(-ret));
        free(new_request);
        goto err;
    }

    LIST_INSERT_HEAD(&xdptf->requests, new_request, link);

    new_request->event_loop_item = event_loop_add_pollable(xdptf->event_loop,
                                                           new_request->pipe_fd, EPOLLIN, true,
                                                           request_fd_event_handler, new_request);

    return 1; /* async */

err:
    return ret;
}

int method_open_file(sd_bus_message *msg, void *data, sd_bus_error *ret_error) {
    struct xdptf *xdptf = data;

    int ret = 0;

    char *handle, *app_id, *parent_window, *title;
    char *current_folder = NULL;
    int multiple = false;
    int directory = false;

    log_print(DEBUG, "method_open_file: fired");

    if ((ret = sd_bus_message_read(msg, "osss", &handle, &app_id, &parent_window, &title)) < 0) {
        log_print(ERROR, "method_open_file: sd_bus_message_read() failed");
        goto err;
    };
    log_print(DEBUG, "method_open_file: handle = %s", handle);
    log_print(DEBUG, "method_open_file: app_id = %s", app_id);
    log_print(DEBUG, "method_open_file: parent_window = %s", parent_window);
    log_print(DEBUG, "method_open_file: title = %s", title);

    if ((ret = sd_bus_message_enter_container(msg, 'a', "{sv}")) < 0) {
        log_print(ERROR, "method_open_file: sd_bus_message_enter_container() failed");
        goto err;
    }
    while (sd_bus_message_enter_container(msg, 'e', "sv") > 0) {
        char *key;

        if ((ret = sd_bus_message_read(msg, "s", &key)) < 0) {
            log_print(ERROR, "method_open_file: sd_bus_message_read() failed");
            goto err;
        }

        if (strcmp(key, "current_folder") == 0) {
            const void *ptr = NULL;
            size_t size = 0;
            if ((ret = sd_bus_message_enter_container(msg, 'v', "ay")) < 0) {
                log_print(ERROR, "method_open_file: sd_bus_message_enter_container() failed");
                goto err;
            }
            if ((ret = sd_bus_message_read_array(msg, 'y', &ptr, &size)) < 0) {
                log_print(ERROR, "method_open_file: sd_bus_message_read_array() failed");
                goto err;
            }
            current_folder = (char *)ptr;
            log_print(DEBUG, "method_open_file: option current_folder = %s", current_folder);
        } else if (strcmp(key, "multiple") == 0) {
            if ((ret = sd_bus_message_read(msg, "v", "b", &multiple)) < 0) {
                log_print(ERROR, "method_open_file: sd_bus_message_read() failed");
                goto err;
            }
            log_print(DEBUG, "method_open_file: option multiple = %d", multiple);
        } else if (strcmp(key, "directory") == 0) {
            if ((ret = sd_bus_message_read(msg, "v", "b", &directory)) < 0) {
                log_print(ERROR, "method_open_file: sd_bus_message_read() failed");
                goto err;
            }
            log_print(DEBUG, "method_open_file: option directory = %d", directory);
        } else {
            log_print(DEBUG, "method_open_file: option %s IGNORED", key);
            if ((ret = sd_bus_message_skip(msg, "v")) < 0) {
                log_print(ERROR, "method_open_file: sd_bus_message_skip() failed");
                goto err;
            }
        }

        if ((ret = sd_bus_message_exit_container(msg)) < 0) {
            log_print(ERROR, "method_open_file: sd_bus_message_exit_container() failed");
            goto err;
        }
    }

    sd_bus_message *response;
    if ((ret = sd_bus_message_new_method_return(msg, &response)) < 0) {
        log_print(ERROR, "sd_bus_message_new_method_return() failed: %s", strerror(-ret));
        goto err;
    }

    struct open_file_request_data request_data = {
        .current_folder = current_folder,
        .directory = directory,
        .multiple = multiple,
    };
    pid_t child_pid;
    ret = exec_picker(xdptf->config.picker_cmd, OPEN_FILE, &request_data, &child_pid);
    if (ret < 0) {
        log_print(ERROR, "exec_picker() failed: %s", strerror(-ret));
        goto err;
    }
    int pipe_fd = ret;

    struct filechooser_request *new_request = xcalloc(1, sizeof(*new_request));
    ds_init(&new_request->buffer);
    new_request->type = OPEN_FILE;
    new_request->response.message = response;
    new_request->pipe_fd = pipe_fd;
    new_request->picker_pid = child_pid;

    if ((ret = sd_bus_add_object_vtable(sd_bus_message_get_bus(msg), &new_request->slot, handle,
                                        interface_name, request_vtable, new_request)) < 0) {
        log_print(ERROR, "sd_bus_add_object_vtable() failed: %s", strerror(-ret));
        free(new_request);
        goto err;
    }

    LIST_INSERT_HEAD(&xdptf->requests, new_request, link);

    new_request->event_loop_item = event_loop_add_pollable(xdptf->event_loop,
                                                           new_request->pipe_fd, EPOLLIN, true,
                                                           request_fd_event_handler, new_request);

    return 1; /* async */

err:
    return ret;
}

void filechooser_request_cleanup(struct filechooser_request *request) {
    LIST_REMOVE(request, link);

    if (request->event_loop_item != NULL) {
        event_loop_remove_callback(request->event_loop_item);
    }

    if (request->slot != NULL) {
        sd_bus_slot_unref(request->slot);
    }

    if (request->response.uris != NULL) {
        for (int i = 0; i < request->response.n_uris; i++) {
            free(request->response.uris[i]);
        }
        free(request->response.uris);
    }

    if (request->response.message != NULL) {
        sd_bus_message_unref(request->response.message);
    }

    ds_free(&request->buffer);

    free(request);
}

