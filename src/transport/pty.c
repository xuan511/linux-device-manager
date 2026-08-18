#define _GNU_SOURCE

#include "devmgr/pty.h"

#include "devmgr/error.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int devmgr_pty_create(int *master_fd, char *slave_path, size_t path_capacity)
{
    int fd;
    char *path;
    size_t path_length;

    if (master_fd == NULL || slave_path == NULL || path_capacity == 0U) {
        return DEVMGR_ERROR_INVALID;
    }
    *master_fd = -1;
    do {
        fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        return DEVMGR_ERROR_IO;
    }
    if (grantpt(fd) < 0 || unlockpt(fd) < 0) {
        (void)close(fd);
        return DEVMGR_ERROR_IO;
    }
    path = ptsname(fd);
    if (path == NULL) {
        (void)close(fd);
        return DEVMGR_ERROR_IO;
    }
    path_length = strlen(path);
    if (path_length + 1U > path_capacity) {
        (void)close(fd);
        return DEVMGR_ERROR_OVERFLOW;
    }
    memcpy(slave_path, path, path_length + 1U);
    *master_fd = fd;
    return DEVMGR_OK;
}
