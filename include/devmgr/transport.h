#ifndef DEVMGR_TRANSPORT_H
#define DEVMGR_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

enum devmgr_transport_kind {
    DEVMGR_TRANSPORT_SERIAL = 0,
    DEVMGR_TRANSPORT_PTY = 1
};

struct devmgr_transport_config {
    enum devmgr_transport_kind kind;
    const char *path;
    unsigned baud_rate;
    bool hardware_flow_control;
};

struct devmgr_transport {
    int fd;
    enum devmgr_transport_kind kind;
};

int devmgr_transport_open(struct devmgr_transport *transport,
                          const struct devmgr_transport_config *config);
ssize_t devmgr_transport_read(struct devmgr_transport *transport, void *buffer, size_t length);
ssize_t devmgr_transport_write(struct devmgr_transport *transport, const void *buffer,
                               size_t length);
int devmgr_transport_get_fd(const struct devmgr_transport *transport);
void devmgr_transport_close(struct devmgr_transport *transport);

#endif

