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
    uint32_t telemetry_interval_ms;
    uint32_t telemetry_samples;
    uint64_t next_telemetry_ns;
    bool telemetry_enabled;
    uint8_t *flash;
    size_t flash_size;
    uint32_t expected_firmware_crc;
    uint32_t firmware_offset;
    uint32_t firmware_session_id;
    uint16_t firmware_chunk_size;
    char firmware_version[32];
    char pending_version[32];
    bool bootloader_active;
    bool firmware_upgrading;
    bool firmware_verified;
    bool running;
    double drop_rate;
    double corrupt_rate;
    uint32_t response_delay_ms;
    int disconnect_at_percent;
    uint64_t outage_until_ns;
    bool outage_injected;
    bool fail_verify;
    uint32_t random_state;
};

struct simulator_config {
    double drop_rate;
    double corrupt_rate;
    uint32_t response_delay_ms;
    int disconnect_at_percent;
    bool fail_verify;
    uint32_t random_seed;
};

int simulator_init(struct simulator_state *simulator, int master_fd,
                   const struct simulator_config *config);
int simulator_run(struct simulator_state *simulator);
void simulator_request_stop(void);
void simulator_cleanup(struct simulator_state *simulator);

#endif
