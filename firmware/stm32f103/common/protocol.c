#include "protocol.h"

#include <string.h>

void stm32_put_le16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
}

void stm32_put_le32(uint8_t *output, uint32_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
    output[2] = (uint8_t)(value >> 16U);
    output[3] = (uint8_t)(value >> 24U);
}

uint16_t stm32_get_le16(const uint8_t *input)
{
    return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8U));
}

uint32_t stm32_get_le32(const uint8_t *input)
{
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8U) | ((uint32_t)input[2] << 16U) |
           ((uint32_t)input[3] << 24U);
}

uint32_t stm32_crc32(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (size_t index = 0U; index < length; ++index) {
        crc ^= bytes[index];
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return crc ^ UINT32_C(0xFFFFFFFF);
}

int stm32_frame_decode(const uint8_t *data, size_t length, struct stm32_frame_view *frame)
{
    if (data == NULL || frame == NULL || length < STM32_DEVMGR_HEADER_SIZE + STM32_DEVMGR_CRC_SIZE)
        return -1;
    if (stm32_get_le16(data) != STM32_DEVMGR_MAGIC || data[2] != STM32_DEVMGR_VERSION ||
        data[5] != 0U) return -2;
    uint16_t payload_length = stm32_get_le16(data + 10U);
    if (payload_length > STM32_DEVMGR_MAX_PAYLOAD ||
        length != STM32_DEVMGR_HEADER_SIZE + payload_length + STM32_DEVMGR_CRC_SIZE) return -3;
    uint32_t expected = stm32_get_le32(data + STM32_DEVMGR_HEADER_SIZE + payload_length);
    if (stm32_crc32(data, STM32_DEVMGR_HEADER_SIZE + payload_length) != expected) return -4;
    frame->type = data[3];
    frame->flags = data[4];
    frame->sequence = stm32_get_le32(data + 6U);
    frame->payload = data + STM32_DEVMGR_HEADER_SIZE;
    frame->payload_length = payload_length;
    return 0;
}

int stm32_frame_encode(uint8_t type, uint8_t flags, uint32_t sequence, const void *payload,
                       uint16_t payload_length, uint8_t *output, size_t capacity,
                       size_t *encoded_length)
{
    size_t total = STM32_DEVMGR_HEADER_SIZE + payload_length + STM32_DEVMGR_CRC_SIZE;
    if (output == NULL || encoded_length == NULL || (payload == NULL && payload_length != 0U) ||
        payload_length > STM32_DEVMGR_MAX_PAYLOAD || capacity < total) return -1;
    stm32_put_le16(output, STM32_DEVMGR_MAGIC);
    output[2] = STM32_DEVMGR_VERSION;
    output[3] = type;
    output[4] = flags;
    output[5] = 0U;
    stm32_put_le32(output + 6U, sequence);
    stm32_put_le16(output + 10U, payload_length);
    if (payload_length != 0U) memcpy(output + STM32_DEVMGR_HEADER_SIZE, payload, payload_length);
    stm32_put_le32(output + STM32_DEVMGR_HEADER_SIZE + payload_length,
                   stm32_crc32(output, STM32_DEVMGR_HEADER_SIZE + payload_length));
    *encoded_length = total;
    return 0;
}

