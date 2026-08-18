#include "devmgr/error.h"
#include "devmgr/log.h"
#include "devmgr/pty.h"
#include "simulator.h"

#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
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
    struct simulator_config config = {.disconnect_at_percent = -1};
    static const struct option options[] = {
        {"drop-rate", required_argument, NULL, 'd'},
        {"corrupt-rate", required_argument, NULL, 'c'},
        {"delay-ms", required_argument, NULL, 'l'},
        {"disconnect-at-percent", required_argument, NULL, 'x'},
        {"fail-verify", no_argument, NULL, 'v'},
        {"seed", required_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};
    int option;
    while ((option = getopt_long(argc, argv, "d:c:l:x:vs:h", options, NULL)) != -1) {
        char *end = NULL;
        switch (option) {
        case 'd': config.drop_rate = strtod(optarg, &end); break;
        case 'c': config.corrupt_rate = strtod(optarg, &end); break;
        case 'l': config.response_delay_ms = (uint32_t)strtoul(optarg, &end, 10); break;
        case 'x': config.disconnect_at_percent = (int)strtol(optarg, &end, 10); break;
        case 'v': config.fail_verify = true; continue;
        case 's': config.random_seed = (uint32_t)strtoul(optarg, &end, 10); break;
        case 'h':
            (void)printf("Usage: %s [--drop-rate 0..1] [--corrupt-rate 0..1] "
                         "[--delay-ms N] [--disconnect-at-percent 0..100] "
                         "[--fail-verify] [--seed N]\n", argv[0]);
            return 0;
        default: return 2;
        }
        if (end == optarg || *end != '\0') return 2;
    }
    if (optind != argc || config.drop_rate < 0.0 || config.drop_rate > 1.0 ||
        config.corrupt_rate < 0.0 || config.corrupt_rate > 1.0 ||
        config.response_delay_ms > 60000U || config.disconnect_at_percent < -1 ||
        config.disconnect_at_percent > 100) return 2;
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
    result = simulator_init(&simulator, master_fd, &config);
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
