#ifndef STM32_DEVMGR_UPDATE_H
#define STM32_DEVMGR_UPDATE_H

#include "protocol.h"

#include <stddef.h>
#include <stdint.h>

enum stm32_update_state {
    STM32_UPDATE_IDLE = 0,
    STM32_UPDATE_RECEIVING,
    STM32_UPDATE_TRANSFERRED,
    STM32_UPDATE_VERIFIED
};

struct stm32_flash_ops {
    int (*erase)(void *context, uint32_t image_size);
    int (*write)(void *context, uint32_t offset, const uint8_t *data, uint16_t length);
    int (*read)(void *context, uint32_t offset, uint8_t *data, uint16_t length);
    uint32_t (*crc32)(void *context, uint32_t image_size);
    int (*activate)(void *context, uint32_t image_size, uint32_t image_crc, const char *version);
    void (*reboot)(void *context);
};

struct stm32_update_context {
    struct stm32_flash_ops flash;
    void *flash_context;
    enum stm32_update_state state;
    uint32_t session_id;
    uint32_t image_size;
    uint32_t image_crc;
    uint32_t next_offset;
    uint16_t chunk_size;
    char pending_version[32];
};

int stm32_update_init(struct stm32_update_context *update, const struct stm32_flash_ops *flash,
                      void *flash_context);
int stm32_update_handle(struct stm32_update_context *update,
                        const struct stm32_frame_view *request, uint8_t *response,
                        size_t response_capacity, size_t *response_length);

#endif

