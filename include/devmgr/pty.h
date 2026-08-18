#ifndef DEVMGR_PTY_H
#define DEVMGR_PTY_H

#include <stddef.h>

int devmgr_pty_create(int *master_fd, char *slave_path, size_t path_capacity);

#endif

