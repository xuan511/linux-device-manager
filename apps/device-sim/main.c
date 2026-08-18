#include "devmgr/error.h"
#include "devmgr/log.h"
#include "devmgr/pty.h"
#include "simulator.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void handle_signal(int signal_number)
{
    (void)signal_number;
    simulator_request_stop();
}

int main(int argc, char **argv)
{
    char slave_path[256];
    int master_fd = -1;
    struct simulator_state simulator;
    struct sigaction action;
    int result;

    (void)argc;
    (void)argv;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    (void)sigemptyset(&action.sa_mask);
    (void)sigaction(SIGINT, &action, NULL);
    (void)sigaction(SIGTERM, &action, NULL);

    result = devmgr_pty_create(&master_fd, slave_path, sizeof(slave_path));
    if (result != DEVMGR_OK) {
        devmgr_log_write(DEVMGR_LOG_ERROR, "sim", "PTY creation failed: %s",
                         devmgr_status_string(result));
        return 1;
    }
    (void)printf("PTY: %s\n", slave_path);
    (void)fflush(stdout);
    result = simulator_init(&simulator, master_fd);
    if (result == DEVMGR_OK) {
        result = simulator_run(&simulator);
    }
    simulator_cleanup(&simulator);
    (void)close(master_fd);
    if (result != DEVMGR_OK) {
        devmgr_log_write(DEVMGR_LOG_ERROR, "sim", "simulator stopped: %s",
                         devmgr_status_string(result));
        return 1;
    }
    return 0;
}
