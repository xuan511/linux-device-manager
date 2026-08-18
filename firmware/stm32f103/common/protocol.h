#ifndef STM32_DEVMGR_PROTOCOL_H
#define STM32_DEVMGR_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define STM32_DEVMGR_MAGIC UINT16_C(0x4D44)
#define STM32_DEVMGR_VERSION UINT8_C(1)
#define STM32_DEVMGR_HEADER_SIZE 12U
#define STM32_DEVMGR_CRC_SIZE 4U
#define STM32_DEVMGR_MAX_PAYLOAD 4096U

#define STM32_MSG_PING UINT8_C(0x01)
#define STM32_MSG_GET_INFO UINT8_C(0x02)
#define STM32_MSG_ENTER_BOOTLOADER UINT8_C(0x10)
#define STM32_MSG_FW_BEGIN UINT8_C(0x11)
#define STM32_MSG_FW_DATA UINT8_C(0x12)
#define STM32_MSG_FW_STATUS UINT8_C(0x13)
#define STM32_MSG_FW_END UINT8_C(0x14)
#define STM32_MSG_FW_VERIFY UINT8_C(0x15)
#define STM32_MSG_FW_ACTIVATE UINT8_C(0x16)
#define STM32_MSG_REBOOT UINT8_C(0x17)
#define STM32_MSG_NACK UINT8_C(0x81)
#define STM32_FRAME_RESPONSE UINT8_C(0x01)

struct stm32_frame_view {
    uint8_t type;
    uint8_t flags;
    uint32_t sequence;
    const uint8_t *payload;
    uint16_t payload_length;
};

void stm32_put_le16(uint8_t *output, uint16_t value);
void stm32_put_le32(uint8_t *output, uint32_t value);
uint16_t stm32_get_le16(const uint8_t *input);
uint32_t stm32_get_le32(const uint8_t *input);
uint32_t stm32_crc32(const void *data, size_t length);
int stm32_frame_decode(const uint8_t *data, size_t length, struct stm32_frame_view *frame);
int stm32_frame_encode(uint8_t type, uint8_t flags, uint32_t sequence, const void *payload,
                       uint16_t payload_length, uint8_t *output, size_t capacity,
                       size_t *encoded_length);

#endif

