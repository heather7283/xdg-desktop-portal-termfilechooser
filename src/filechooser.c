#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

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

static LIST_HEAD(requests, filechooser_request) requests = LIST_HEAD_INITIALIZER(requests);

static int send_response_error(struct filechooser_request *request) {
    int ret = 0;
    struct sd_bus_message *reply = request->response.message;

    if ((ret = sd_bus_message_append(reply, "u", PORTAL_RESPONSE_ENDED, 1)) < 0) {
        log_print(ERROR, "sd_bus_message_append() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_open_container(reply, 'a', "{sv}")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_send(NULL, reply, NULL)) < 0) {
        log_print(ERROR, "sd_bus_send() failed: %s", strerror(-ret));
        goto cleanup;
    }
cleanup:
    filechooser_request_cleanup(request);
    return ret;
}

static int send_response_cancelled(struct filechooser_request *request) {
    int ret = 0;
    struct sd_bus_message *reply = request->response.message;

    if ((ret = sd_bus_message_append(reply, "u", PORTAL_RESPONSE_CANCELLED, 1)) < 0) {
        log_print(ERROR, "sd_bus_message_append() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_open_container(reply, 'a', "{sv}")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_send(NULL, reply, NULL)) < 0) {
        log_print(ERROR, "sd_bus_send() failed: %s", strerror(-ret));
        goto cleanup;
    }
cleanup:
    filechooser_request_cleanup(request);
    return ret;
}

static int send_response_success(struct filechooser_request *request) {
    int ret = 0;
    struct sd_bus_message *reply = request->response.message;

    if ((ret = sd_bus_message_append(reply, "u", PORTAL_RESPONSE_SUCCESS, 1)) < 0) {
        log_print(ERROR, "sd_bus_message_append() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_open_container(reply, 'a', "{sv}")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_open_container(reply, 'e', "sv")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_append_basic(reply, 's', "uris")) < 0) {
        log_print(ERROR, "sd_bus_message_append_basic() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_open_container(reply, 'v', "as")) < 0) {
        log_print(ERROR, "sd_bus_message_open_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_append_strv(reply, request->response.uris)) < 0) {
        log_print(ERROR, "sd_bus_message_append_strv() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_message_close_container(reply)) < 0) {
        log_print(ERROR, "sd_bus_message_close_container() failed: %s", strerror(-ret));
        goto cleanup;
    }
    if ((ret = sd_bus_send(NULL, reply, NULL)) < 0) {
        log_print(ERROR, "sd_bus_send() failed: %s", strerror(-ret));
        goto cleanup;
    }
cleanup:
    filechooser_request_cleanup(request);
    return ret;
}

static int request_fd_event_handler(struct event_loop *loop, struct event_loop_item *item) {
    struct filechooser_request *request = item->data;

    static char buf[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(request->pipe_fd, buf, sizeof(buf))) > 0) {
        ds_append_bytes(&request->buffer, buf, bytes_read);
    }

    if (bytes_read == -1) {
        log_print(ERROR, "failed to read from pipe (fd %d): %s", request->pipe_fd, strerror(errno));
        event_loop_remove_item(loop, item);
        send_response_error(request);
        return -1;
    } else if (bytes_read == 0) {
        log_print(DEBUG, "EOF on pipe fd %d", request->pipe_fd);
        event_loop_remove_item(loop, item);

        ds_append_bytes(&request->buffer, "", sizeof(""));
        /* TODO: check number of uris returned when only one uri is needed */
        char **uris;
        int n_uris = get_uris_from_string(request->buffer.data, &uris);

        log_print(DEBUG, "got %d uris", n_uris);

        if (n_uris == 0) {
            return send_response_cancelled(request);
        } else {
            request->response.n_uris = n_uris;
            request->response.uris = uris;
            return send_response_success(request);
        }
    }

    return 0;
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
    ret = exec_picker(xdptf->config.picker_cmd, SAVE_FILE, &request_data);
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

    LIST_INSERT_HEAD(&requests, new_request, link);

    event_loop_add_item(&xdptf->event_loop,
                        new_request->pipe_fd, request_fd_event_handler, new_request);

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
    ret = exec_picker(xdptf->config.picker_cmd, OPEN_FILE, &request_data);
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

    LIST_INSERT_HEAD(&requests, new_request, link);

    event_loop_add_item(&xdptf->event_loop,
                        new_request->pipe_fd, request_fd_event_handler, new_request);

    return 1; /* async */

err:
    return ret;
}

void filechooser_request_cleanup(struct filechooser_request *request) {
    LIST_REMOVE(request, link);

    /* you can never have too many NULL checks */
    if (request->response.uris != NULL) {
        for (int i = 0; i < request->response.n_uris; i++) {
            if (request->response.uris[i] != NULL) {
                free(request->response.uris[i]);
            }
        }
        free(request->response.uris);
    }

    if (request->response.message != NULL) {
        sd_bus_message_unref(request->response.message);
    }

    ds_free(&request->buffer);

    free(request);
}

void filechooser_requests_cleanup(void) {
    struct filechooser_request *request, *request_tmp;
    LIST_FOREACH_SAFE(request, &requests, link, request_tmp) {
        filechooser_request_cleanup(request);
    };
}

