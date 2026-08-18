#ifndef DEVMGR_PROTOCOL_H
#define DEVMGR_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define DEVMGR_PROTOCOL_MAGIC UINT16_C(0x4D44)
#define DEVMGR_PROTOCOL_VERSION UINT8_C(1)
#define DEVMGR_PROTOCOL_HEADER_SIZE 12U
#define DEVMGR_PROTOCOL_CRC_SIZE 4U
#define DEVMGR_PROTOCOL_MAX_PAYLOAD 4096U
#define DEVMGR_PROTOCOL_MAX_FRAME                                                               \
    (DEVMGR_PROTOCOL_HEADER_SIZE + DEVMGR_PROTOCOL_MAX_PAYLOAD + DEVMGR_PROTOCOL_CRC_SIZE)

enum devmgr_message_type {
    DEVMGR_MSG_PING = 0x01,
    DEVMGR_MSG_GET_INFO = 0x02,
    DEVMGR_MSG_GET_HEALTH = 0x03,
    DEVMGR_MSG_GET_STATS = 0x04,
    DEVMGR_MSG_START_TELEMETRY = 0x05,
    DEVMGR_MSG_STOP_TELEMETRY = 0x06,
    DEVMGR_MSG_ENTER_BOOTLOADER = 0x10,
    DEVMGR_MSG_FW_BEGIN = 0x11,
    DEVMGR_MSG_FW_DATA = 0x12,
    DEVMGR_MSG_FW_STATUS = 0x13,
    DEVMGR_MSG_FW_END = 0x14,
    DEVMGR_MSG_FW_VERIFY = 0x15,
    DEVMGR_MSG_FW_ACTIVATE = 0x16,
    DEVMGR_MSG_REBOOT = 0x17,
    DEVMGR_MSG_ACK = 0x80,
    DEVMGR_MSG_NACK = 0x81,
    DEVMGR_MSG_ERROR = 0x82,
    DEVMGR_MSG_TELEMETRY = 0x83
};

enum devmgr_frame_flags {
    DEVMGR_FRAME_RESPONSE = 1U << 0,
    DEVMGR_FRAME_ACK_REQUIRED = 1U << 1,
    DEVMGR_FRAME_RETRY = 1U << 2
};

struct devmgr_frame {
    uint8_t type;
    uint8_t flags;
    uint32_t sequence;
    uint16_t payload_length;
    uint8_t payload[DEVMGR_PROTOCOL_MAX_PAYLOAD];
};

void devmgr_put_le16(uint8_t *output, uint16_t value);
void devmgr_put_le32(uint8_t *output, uint32_t value);
uint16_t devmgr_get_le16(const uint8_t *input);
uint32_t devmgr_get_le32(const uint8_t *input);

int devmgr_frame_encode(const struct devmgr_frame *frame, uint8_t *output, size_t capacity,
                        size_t *encoded_length);
int devmgr_frame_decode(const uint8_t *data, size_t length, struct devmgr_frame *frame);

#endif

