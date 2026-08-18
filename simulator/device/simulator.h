#ifndef DEVMGR_SIMULATOR_H
#define DEVMGR_SIMULATOR_H

#include "devmgr/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

struct simulator_state {
    int master_fd;
    struct devmgr_parser parser;
    struct timespec started_at;
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t errors;
    uint32_t device_id;
    int32_t temperature_millic;
    uint32_t voltage_mv;
    bool running;
};

int simulator_init(struct simulator_state *simulator, int master_fd);
int simulator_run(struct simulator_state *simulator);
void simulator_request_stop(void);

#endif

