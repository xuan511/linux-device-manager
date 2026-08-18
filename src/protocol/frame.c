#include "devmgr/protocol.h"

#include "devmgr/crc32.h"
#include "devmgr/error.h"

#include <string.h>

void devmgr_put_le16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value & UINT16_C(0x00FF));
    output[1] = (uint8_t)(value >> 8U);
}

void devmgr_put_le32(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)(value & UINT32_C(0x000000FF));
    output[1] = (uint8_t)((value >> 8U) & UINT32_C(0x000000FF));
    output[2] = (uint8_t)((value >> 16U) & UINT32_C(0x000000FF));
    output[3] = (uint8_t)(value >> 24U);
}

uint16_t devmgr_get_le16(const uint8_t *input)
{
    return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8U));
}

uint32_t devmgr_get_le32(const uint8_t *input)
{
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8U) | ((uint32_t)input[2] << 16U) |
           ((uint32_t)input[3] << 24U);
}

int devmgr_frame_encode(const struct devmgr_frame *frame, uint8_t *output, size_t capacity,
                        size_t *encoded_length)
{
    size_t frame_length;
    uint32_t crc;

    if (frame == NULL || output == NULL || encoded_length == NULL ||
        frame->payload_length > DEVMGR_PROTOCOL_MAX_PAYLOAD) {
        return DEVMGR_ERROR_INVALID;
    }
    frame_length = DEVMGR_PROTOCOL_HEADER_SIZE + frame->payload_length + DEVMGR_PROTOCOL_CRC_SIZE;
    if (capacity < frame_length) {
        return DEVMGR_ERROR_OVERFLOW;
    }
    devmgr_put_le16(output, DEVMGR_PROTOCOL_MAGIC);
    output[2] = DEVMGR_PROTOCOL_VERSION;
    output[3] = frame->type;
    output[4] = frame->flags;
    output[5] = 0U;
    devmgr_put_le32(output + 6U, frame->sequence);
    devmgr_put_le16(output + 10U, frame->payload_length);
    memcpy(output + DEVMGR_PROTOCOL_HEADER_SIZE, frame->payload, frame->payload_length);
    crc = devmgr_crc32(output, DEVMGR_PROTOCOL_HEADER_SIZE + frame->payload_length);
    devmgr_put_le32(output + DEVMGR_PROTOCOL_HEADER_SIZE + frame->payload_length, crc);
    *encoded_length = frame_length;
    return DEVMGR_OK;
}

int devmgr_frame_decode(const uint8_t *data, size_t length, struct devmgr_frame *frame)
{
    uint16_t payload_length;
    size_t expected_length;
    uint32_t expected_crc;
    uint32_t actual_crc;

    if (data == NULL || frame == NULL || length < DEVMGR_PROTOCOL_HEADER_SIZE + DEVMGR_PROTOCOL_CRC_SIZE) {
        return DEVMGR_ERROR_INVALID;
    }
    if (devmgr_get_le16(data) != DEVMGR_PROTOCOL_MAGIC || data[2] != DEVMGR_PROTOCOL_VERSION ||
        data[5] != 0U) {
        return DEVMGR_ERROR_PROTOCOL;
    }
    payload_length = devmgr_get_le16(data + 10U);
    if (payload_length > DEVMGR_PROTOCOL_MAX_PAYLOAD) {
        return DEVMGR_ERROR_LIMIT;
    }
    expected_length = DEVMGR_PROTOCOL_HEADER_SIZE + payload_length + DEVMGR_PROTOCOL_CRC_SIZE;
    if (length != expected_length) {
        return DEVMGR_ERROR_INVALID;
    }
    expected_crc = devmgr_get_le32(data + DEVMGR_PROTOCOL_HEADER_SIZE + payload_length);
    actual_crc = devmgr_crc32(data, DEVMGR_PROTOCOL_HEADER_SIZE + payload_length);
    if (actual_crc != expected_crc) {
        return DEVMGR_ERROR_CRC;
    }
    frame->type = data[3];
    frame->flags = data[4];
    frame->sequence = devmgr_get_le32(data + 6U);
    frame->payload_length = payload_length;
    memcpy(frame->payload, data + DEVMGR_PROTOCOL_HEADER_SIZE, payload_length);
    return DEVMGR_OK;
}

