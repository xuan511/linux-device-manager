#include "devmgr/transport.h"

#include "devmgr/error.h"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static int baud_to_speed(unsigned baud_rate, speed_t *speed)
{
    if (speed == NULL) {
        return DEVMGR_ERROR_INVALID;
    }
    switch (baud_rate) {
    case 9600U: *speed = B9600; break;
    case 19200U: *speed = B19200; break;
    case 38400U: *speed = B38400; break;
    case 57600U: *speed = B57600; break;
    case 115200U: *speed = B115200; break;
    default: return DEVMGR_ERROR_INVALID;
    }
    return DEVMGR_OK;
}

static int configure_serial(int fd, const struct devmgr_transport_config *config)
{
    struct termios attributes;
    speed_t speed;

    if (baud_to_speed(config->baud_rate, &speed) != DEVMGR_OK) {
        return DEVMGR_ERROR_INVALID;
    }
    if (tcgetattr(fd, &attributes) < 0) {
        return DEVMGR_ERROR_IO;
    }
    cfmakeraw(&attributes);
    attributes.c_cflag |= CLOCAL | CREAD;
    attributes.c_cflag &= (tcflag_t)~(PARENB | CSTOPB | CSIZE);
    attributes.c_cflag |= CS8;
#ifdef CRTSCTS
    if (config->hardware_flow_control) {
        attributes.c_cflag |= CRTSCTS;
    } else {
        attributes.c_cflag &= (tcflag_t)~CRTSCTS;
    }
#else
    if (config->hardware_flow_control) {
        return DEVMGR_ERROR_INVALID;
    }
#endif
    attributes.c_iflag &= (tcflag_t)~(IXON | IXOFF | IXANY);
    attributes.c_cc[VMIN] = 0;
    attributes.c_cc[VTIME] = 0;
    if (cfsetispeed(&attributes, speed) < 0 || cfsetospeed(&attributes, speed) < 0) {
        return DEVMGR_ERROR_IO;
    }
    if (tcsetattr(fd, TCSANOW, &attributes) < 0) {
        return DEVMGR_ERROR_IO;
    }
    return DEVMGR_OK;
}

int devmgr_transport_open(struct devmgr_transport *transport,
                          const struct devmgr_transport_config *config)
{
    int fd;
    int result;

    if (transport == NULL || config == NULL || config->path == NULL ||
        (config->kind != DEVMGR_TRANSPORT_SERIAL && config->kind != DEVMGR_TRANSPORT_PTY)) {
        return DEVMGR_ERROR_INVALID;
    }
    transport->fd = -1;
    do {
        fd = open(config->path, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        return DEVMGR_ERROR_IO;
    }
    result = configure_serial(fd, config);
    if (result != DEVMGR_OK) {
        (void)close(fd);
        return result;
    }
    transport->fd = fd;
    transport->kind = config->kind;
    return DEVMGR_OK;
}

ssize_t devmgr_transport_read(struct devmgr_transport *transport, void *buffer, size_t length)
{
    ssize_t count;
    if (transport == NULL || transport->fd < 0 || (buffer == NULL && length != 0U)) {
        errno = EINVAL;
        return -1;
    }
    do {
        count = read(transport->fd, buffer, length);
    } while (count < 0 && errno == EINTR);
    return count;
}

ssize_t devmgr_transport_write(struct devmgr_transport *transport, const void *buffer,
                               size_t length)
{
    ssize_t count;
    if (transport == NULL || transport->fd < 0 || (buffer == NULL && length != 0U)) {
        errno = EINVAL;
        return -1;
    }
    do {
        count = write(transport->fd, buffer, length);
    } while (count < 0 && errno == EINTR);
    return count;
}

int devmgr_transport_get_fd(const struct devmgr_transport *transport)
{
    return transport == NULL ? -1 : transport->fd;
}

void devmgr_transport_close(struct devmgr_transport *transport)
{
    if (transport != NULL && transport->fd >= 0) {
        int result;
        do {
            result = close(transport->fd);
        } while (result < 0 && errno == EINTR);
        transport->fd = -1;
    }
}

