#include "update.h"

#include <string.h>

enum { NACK_INVALID_STATE = 2, NACK_INVALID_PAYLOAD = 3, NACK_OFFSET = 5,
       NACK_STORAGE = 6, NACK_VERIFY = 7 };

static int respond(const struct stm32_frame_view *request, uint8_t type, const uint8_t *payload,
                   uint16_t payload_length, uint8_t *output, size_t capacity, size_t *length)
{
    return stm32_frame_encode(type, STM32_FRAME_RESPONSE, request->sequence, payload,
                              payload_length, output, capacity, length);
}

static int nack(const struct stm32_frame_view *request, uint16_t error, uint8_t *output,
                size_t capacity, size_t *length)
{
    uint8_t payload[2];
    stm32_put_le16(payload, error);
    return respond(request, STM32_MSG_NACK, payload, sizeof(payload), output, capacity, length);
}

int stm32_update_init(struct stm32_update_context *update, const struct stm32_flash_ops *flash,
                      void *flash_context)
{
    if (update == NULL || flash == NULL || flash->erase == NULL || flash->write == NULL ||
        flash->read == NULL || flash->crc32 == NULL || flash->activate == NULL ||
        flash->reboot == NULL) return -1;
    memset(update, 0, sizeof(*update));
    update->flash = *flash;
    update->flash_context = flash_context;
    update->session_id = 1U;
    return 0;
}

static int duplicate_matches(struct stm32_update_context *update, uint32_t offset,
                             const uint8_t *data, uint16_t length)
{
    uint8_t scratch[32];
    uint16_t checked = 0U;
    while (checked < length) {
        uint16_t remaining = (uint16_t)(length - checked);
        uint16_t count = remaining < sizeof(scratch) ? remaining : (uint16_t)sizeof(scratch);
        if (update->flash.read(update->flash_context, offset + checked, scratch, count) != 0 ||
            memcmp(scratch, data + checked, count) != 0) return 0;
        checked = (uint16_t)(checked + count);
    }
    return 1;
}

static int handle_begin(struct stm32_update_context *update,
                        const struct stm32_frame_view *request, uint8_t *response,
                        size_t capacity, size_t *length)
{
    if (request->payload_length < 11U) return nack(request, NACK_INVALID_PAYLOAD, response, capacity, length);
    uint32_t size = stm32_get_le32(request->payload);
    uint32_t crc = stm32_get_le32(request->payload + 4U);
    uint16_t chunk = stm32_get_le16(request->payload + 8U);
    const uint8_t *end = memchr(request->payload + 10U, 0, request->payload_length - 10U);
    size_t version_length = end == NULL ? 0U : (size_t)(end - (request->payload + 10U));
    if (size == 0U || chunk == 0U || version_length == 0U ||
        version_length >= sizeof(update->pending_version))
        return nack(request, NACK_INVALID_PAYLOAD, response, capacity, length);
    if (update->state == STM32_UPDATE_RECEIVING && size == update->image_size &&
        crc == update->image_crc && strcmp((const char *)request->payload + 10U,
                                           update->pending_version) == 0) {
        uint8_t payload[8];
        stm32_put_le32(payload, update->session_id);
        stm32_put_le32(payload + 4U, update->next_offset);
        return respond(request, request->type, payload, sizeof(payload), response, capacity, length);
    }
    if (update->flash.erase(update->flash_context, size) != 0)
        return nack(request, NACK_STORAGE, response, capacity, length);
    ++update->session_id;
    if (update->session_id == 0U) update->session_id = 1U;
    update->image_size = size;
    update->image_crc = crc;
    update->chunk_size = chunk;
    update->next_offset = 0U;
    memcpy(update->pending_version, request->payload + 10U, version_length + 1U);
    update->state = STM32_UPDATE_RECEIVING;
    uint8_t payload[8];
    stm32_put_le32(payload, update->session_id);
    stm32_put_le32(payload + 4U, 0U);
    return respond(request, request->type, payload, sizeof(payload), response, capacity, length);
}

static int handle_data(struct stm32_update_context *update,
                       const struct stm32_frame_view *request, uint8_t *response,
                       size_t capacity, size_t *length)
{
    if (update->state != STM32_UPDATE_RECEIVING || request->payload_length <= 8U ||
        stm32_get_le32(request->payload) != update->session_id)
        return nack(request, NACK_INVALID_STATE, response, capacity, length);
    uint32_t offset = stm32_get_le32(request->payload + 4U);
    uint16_t data_length = (uint16_t)(request->payload_length - 8U);
    if (data_length > update->chunk_size || offset > update->image_size ||
        data_length > update->image_size - offset)
        return nack(request, NACK_OFFSET, response, capacity, length);
    if (offset < update->next_offset) {
        if (offset + data_length > update->next_offset ||
            !duplicate_matches(update, offset, request->payload + 8U, data_length))
            return nack(request, NACK_OFFSET, response, capacity, length);
    } else if (offset == update->next_offset) {
        if (update->flash.write(update->flash_context, offset, request->payload + 8U,
                                data_length) != 0)
            return nack(request, NACK_STORAGE, response, capacity, length);
        update->next_offset += data_length;
    } else return nack(request, NACK_OFFSET, response, capacity, length);
    uint8_t payload[4];
    stm32_put_le32(payload, update->next_offset);
    return respond(request, request->type, payload, sizeof(payload), response, capacity, length);
}

int stm32_update_handle(struct stm32_update_context *update,
                        const struct stm32_frame_view *request, uint8_t *response,
                        size_t response_capacity, size_t *response_length)
{
    if (update == NULL || request == NULL || response == NULL || response_length == NULL) return -1;
    uint8_t payload[8];
    switch (request->type) {
    case STM32_MSG_ENTER_BOOTLOADER:
        return respond(request, request->type, NULL, 0U, response, response_capacity, response_length);
    case STM32_MSG_FW_BEGIN:
        return handle_begin(update, request, response, response_capacity, response_length);
    case STM32_MSG_FW_DATA:
        return handle_data(update, request, response, response_capacity, response_length);
    case STM32_MSG_FW_STATUS:
        if (request->payload_length != 4U || stm32_get_le32(request->payload) != update->session_id)
            return nack(request, NACK_INVALID_STATE, response, response_capacity, response_length);
        stm32_put_le32(payload, update->session_id);
        stm32_put_le32(payload + 4U, update->next_offset);
        return respond(request, request->type, payload, 8U, response, response_capacity, response_length);
    case STM32_MSG_FW_END:
        if (update->state != STM32_UPDATE_RECEIVING || update->next_offset != update->image_size)
            return nack(request, NACK_OFFSET, response, response_capacity, response_length);
        update->state = STM32_UPDATE_TRANSFERRED;
        return respond(request, request->type, NULL, 0U, response, response_capacity, response_length);
    case STM32_MSG_FW_VERIFY:
        if (update->state != STM32_UPDATE_TRANSFERRED ||
            update->flash.crc32(update->flash_context, update->image_size) != update->image_crc)
            return nack(request, NACK_VERIFY, response, response_capacity, response_length);
        update->state = STM32_UPDATE_VERIFIED;
        stm32_put_le32(payload, update->image_crc);
        return respond(request, request->type, payload, 4U, response, response_capacity, response_length);
    case STM32_MSG_FW_ACTIVATE:
        if (update->state != STM32_UPDATE_VERIFIED ||
            update->flash.activate(update->flash_context, update->image_size, update->image_crc,
                                   update->pending_version) != 0)
            return nack(request, NACK_INVALID_STATE, response, response_capacity, response_length);
        return respond(request, request->type, NULL, 0U, response, response_capacity, response_length);
    case STM32_MSG_REBOOT: {
        int result = respond(request, request->type, NULL, 0U, response, response_capacity,
                             response_length);
        if (result == 0) update->flash.reboot(update->flash_context);
        return result;
    }
    default: return nack(request, NACK_INVALID_PAYLOAD, response, response_capacity, response_length);
    }
}

