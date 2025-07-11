#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "picker.h"
#include "filechooser.h"
#include "log.h"

enum {
    PIPE_READING_END = 0,
    PIPE_WRITING_END = 1,
};

int exec_picker(const char *exe,
                enum filechooser_request_type request_type, void *request_data, pid_t *child_pid) {
    int ret = 0;
    int pipe_fds[2] = {-1, -1};

    if (pipe(pipe_fds) < 0) {
        ret = -errno;
        log_print(ERROR, "failed to create pipe: %s", strerror(errno));
        goto err;
    }

    int flags = fcntl(pipe_fds[PIPE_READING_END], F_GETFL, 0);
    if (flags < 0) {
        ret = -errno;
        log_print(ERROR, "fcntl() on fd %d failed: %s",
                  pipe_fds[PIPE_READING_END], strerror(errno));
        goto err;
    }
    if (fcntl(pipe_fds[PIPE_READING_END], F_SETFL, flags | O_NONBLOCK) < 0) {
        ret = -errno;
        log_print(ERROR, "failed to set O_NONBLOCK on fd %d: %s",
                  pipe_fds[PIPE_READING_END], strerror(errno));
        goto err;
    }

    flags = fcntl(pipe_fds[PIPE_READING_END], F_GETFD, 0);
    if (flags < 0) {
        ret = -errno;
        log_print(ERROR, "fcntl() on fd %d failed: %s",
                  pipe_fds[PIPE_READING_END], strerror(errno));
        goto err;
    }
    if (fcntl(pipe_fds[PIPE_READING_END], F_SETFD, flags | FD_CLOEXEC) < 0) {
        ret = -errno;
        log_print(ERROR, "failed to set FD_CLOEXEC on fd %d: %s",
                  pipe_fds[PIPE_READING_END], strerror(errno));
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

        if (dup2(pipe_fds[PIPE_WRITING_END], 4) < 0) {
            log_print(ERROR, "picker: failed to duplicate pipe fd to fd 4: %s", strerror(errno));
            exit(1);
        };
        close(pipe_fds[PIPE_WRITING_END]);

        if (setpgid(0, 0) < 0) {
            log_print(WARN, "setpgid() failed: %s, won't be able to kill picker", strerror(errno));
        }

        switch (request_type) {
        case SAVE_FILE: {
            struct save_file_request_data *data = request_data;
            const char *current_name = data->current_name;
            const char *current_folder = data->current_folder;
            log_print(DEBUG, "picker: executing %s %d %s %s",
                      exe, SAVE_FILE, current_folder, current_name);
            execlp(exe, exe,
                   "0", /* SAVE_FILE */
                   (current_folder != NULL) ? current_folder : "/tmp",
                   (current_name != NULL) ? current_name : "FALLBACK_FILENAME",
                   NULL);
            break;
        }
        case OPEN_FILE: {
            struct open_file_request_data *data = request_data;
            const char *current_folder = data->current_folder;
            int multiple = data->multiple;
            int directory = data->directory;
            log_print(DEBUG, "picker: executing %s %d %s %d %d",
                      exe, OPEN_FILE, current_folder, multiple, directory);
            execlp(exe, exe,
                   "2", /* OPEN_FILE */
                   (current_folder != NULL) ? current_folder : "/tmp",
                   multiple ? "1" : "0",
                   directory ? "1" : "0",
                   NULL);
            break;
        }
        case SAVE_FILES:
            log_print(ERROR, "TODO: not implemented exec_picker() for this request type");
            abort();
            break;
        default:
            log_print(ERROR, "UNREACHABLE: illegal request type");
            abort();
        }

        /* exec only returns on error */
        log_print(ERROR, "child: execlp() failed: %s", strerror(errno));
        exit(1);
        break;
    default:
        /* parent continues... */
        close(pipe_fds[PIPE_WRITING_END]);
        log_print(DEBUG, "forked child with pid %d", pid);
        *child_pid = pid;
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

