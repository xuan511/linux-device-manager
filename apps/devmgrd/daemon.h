#ifndef DEVMGR_DAEMON_H
#define DEVMGR_DAEMON_H

struct devmgr_daemon_config {
    const char *device_path;
    const char *socket_path;
};

int devmgr_daemon_run(const struct devmgr_daemon_config *config);

#endif

