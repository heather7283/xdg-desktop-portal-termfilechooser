#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "picker.h"
#include "filechooser.h"
#include "log.h"

enum {
    PIPE_READING_END = 0,
    PIPE_WRITING_END = 1,
};

int exec_picker(const char *exe, enum filechooser_request_type request_type, void *request_data) {
    int ret = 0;
    int pipe_fds[2] = {-1, -1};

    if (pipe(pipe_fds) < 0) {
        ret = -errno;
        log_print(ERROR, "failed to create pipe: %s", strerror(errno));
        goto err;
    }

    pid_t pid;
    switch (pid = fork()) {
    case -1:
        ret = -errno;
        log_print(ERROR, "fork failed: %s", strerror(errno));
        goto err;
    case 0:
        /* child */
        close(pipe_fds[PIPE_READING_END]);

        /* INT_MAX is 2147483647 (10 chars) + null terminator = 11 */
        char pipe_fd_string[11];
        snprintf(pipe_fd_string, sizeof(pipe_fd_string) / sizeof(pipe_fd_string[0]),
                 "%d", pipe_fds[PIPE_WRITING_END]);

        switch (request_type) {
        case SAVE_FILE:
            struct save_file_request_data *data = request_data;
            const char *current_name = data->current_name;
            const char *current_folder = data->current_folder;
            log_print(TRACE, "picker: executing %s %s %d %s %s",
                      exe, pipe_fd_string, SAVE_FILE, current_folder, current_name);
            execlp(exe, exe,
                   pipe_fd_string,
                   "0", /* SAVE_FILE */
                   (current_folder != NULL) ? current_folder : "/tmp",
                   (current_name != NULL) ? current_name : "FALLBACK_FILENAME",
                   NULL);
            break;
        case SAVE_FILES:
        case OPEN_FILE:
            die("TODO: not implemented exec_picker() for this request type");
            break;
        default:
            die("UNREACHABLE: illegal request type");
        }

        /* exec only returns on error */
        log_print(ERROR, "child: execlp() failed: %s", strerror(errno));
        exit(1);
        break;
    default:
        /* parent continues... */
        close(pipe_fds[PIPE_WRITING_END]);
        log_print(DEBUG, "forked child with pid %d", pid);
        break;
    }

    return pipe_fds[PIPE_READING_END];

err:
    if (pipe_fds[PIPE_READING_END] > 0) {
        close(pipe_fds[PIPE_READING_END]);
    }
    if (pipe_fds[PIPE_WRITING_END] > 0) {
        close(pipe_fds[PIPE_WRITING_END]);
    }
    return ret;
}

