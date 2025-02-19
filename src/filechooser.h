#ifndef FILECHOOSER_H
#define FILECHOOSER_H

#include <stdbool.h>

#include "thirdparty/queue.h"
#include "sd-bus.h"
#include "da.h"

enum filechooser_request_type {
    SAVE_FILE = 0,
    SAVE_FILES = 1,
    OPEN_FILE = 2,
};

struct save_file_request_data {
    /* suggested name of the file */
    char *current_name;
    /* suggested folder in which the file should be saved */
    char *current_folder;
};

struct save_files_request_data {
    /* an array of file names to be saved */
    char **files;
    /* suggested folder in which the file should be saved */
    char *current_folder;
};

struct open_file_request_data {
    /* whether multiple files can be selected or not */
    bool multiple;
    /* whether to select for folders instead of files */
    bool directory;
    /* suggested folder in which the file should be saved */
    char *current_folder;
};

struct filechooser_request {
    enum filechooser_request_type type;

    struct {
        sd_bus_message *message;

        int n_uris;
        char **uris;
    } response;

    int pipe_fd;
    pid_t picker_pid;
    struct da buffer;

    LIST_ENTRY(filechooser_request) link;
};

int method_save_file(sd_bus_message *msg, void *data, sd_bus_error *ret_error);
int method_open_file(sd_bus_message *msg, void *data, sd_bus_error *ret_error);
void filechooser_request_cleanup(struct filechooser_request *request);
void filechooser_requests_cleanup(void);

#endif /* ifndef FILECHOOSER_H */

