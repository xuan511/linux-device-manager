#ifndef DEVMGR_UPGRADE_H
#define DEVMGR_UPGRADE_H

#include "devmgr/protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEVMGR_UPGRADE_VERSION_MAX 31U
#define DEVMGR_UPGRADE_DEFAULT_CHUNK 1024U

enum devmgr_upgrade_state {
    DEVMGR_UPGRADE_IDLE = 0,
    DEVMGR_UPGRADE_ENTER_BOOTLOADER,
    DEVMGR_UPGRADE_BEGIN,
    DEVMGR_UPGRADE_TRANSFER,
    DEVMGR_UPGRADE_RECOVER,
    DEVMGR_UPGRADE_END,
    DEVMGR_UPGRADE_VERIFY,
    DEVMGR_UPGRADE_ACTIVATE,
    DEVMGR_UPGRADE_REBOOT,
    DEVMGR_UPGRADE_COMPLETE,
    DEVMGR_UPGRADE_ERROR
};

struct devmgr_upgrade {
    enum devmgr_upgrade_state state;
    const uint8_t *image_data;
    size_t image_size;
    uint32_t image_crc;
    char version[DEVMGR_UPGRADE_VERSION_MAX + 1U];
    uint32_t session_id;
    uint32_t offset;
    uint32_t last_chunk_end;
    uint16_t chunk_size;
    uint32_t chunks_sent;
    uint8_t recovery_attempts;
};

void devmgr_upgrade_init(struct devmgr_upgrade *upgrade);
int devmgr_upgrade_start(struct devmgr_upgrade *upgrade, const uint8_t *image_data,
                         size_t image_size, uint32_t image_crc, const char *version,
                         uint16_t chunk_size);
int devmgr_upgrade_build_request(struct devmgr_upgrade *upgrade, struct devmgr_frame *frame);
int devmgr_upgrade_accept_response(struct devmgr_upgrade *upgrade, uint8_t request_type,
                                   const struct devmgr_frame *response);
bool devmgr_upgrade_active(const struct devmgr_upgrade *upgrade);
int devmgr_upgrade_begin_recovery(struct devmgr_upgrade *upgrade);
const char *devmgr_upgrade_state_string(enum devmgr_upgrade_state state);

#endif
