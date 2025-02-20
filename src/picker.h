#ifndef PICKER_H
#define PICKER_H

#include "filechooser.h"

/* returns pipe fd on success, negative errno retcode on failure */
int exec_picker(const char *exe,
                enum filechooser_request_type request_type, void *request_data, pid_t *child_pid);

#endif /* #ifndef PICKER_H */

