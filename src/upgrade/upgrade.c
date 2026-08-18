#include "devmgr/upgrade.h"

#include "devmgr/error.h"

#include <limits.h>
#include <string.h>

void devmgr_upgrade_init(struct devmgr_upgrade *upgrade)
{
    if (upgrade != NULL) {
        memset(upgrade, 0, sizeof(*upgrade));
        upgrade->state = DEVMGR_UPGRADE_IDLE;
    }
}

int devmgr_upgrade_start(struct devmgr_upgrade *upgrade, const uint8_t *image_data,
                         size_t image_size, uint32_t image_crc, const char *version,
                         uint16_t chunk_size)
{
    size_t version_length;
    if (upgrade == NULL || image_data == NULL || image_size == 0U || image_size > UINT32_MAX ||
        version == NULL || chunk_size == 0U ||
        chunk_size > DEVMGR_PROTOCOL_MAX_PAYLOAD - 8U) return DEVMGR_ERROR_INVALID;
    version_length = strlen(version);
    if (version_length == 0U || version_length > DEVMGR_UPGRADE_VERSION_MAX) return DEVMGR_ERROR_INVALID;
    devmgr_upgrade_init(upgrade);
    upgrade->image_data = image_data;
    upgrade->image_size = image_size;
    upgrade->image_crc = image_crc;
    memcpy(upgrade->version, version, version_length + 1U);
    upgrade->chunk_size = chunk_size;
    upgrade->state = DEVMGR_UPGRADE_ENTER_BOOTLOADER;
    return DEVMGR_OK;
}

int devmgr_upgrade_build_request(struct devmgr_upgrade *upgrade, struct devmgr_frame *frame)
{
    size_t remaining;
    size_t chunk;
    size_t version_length;
    if (upgrade == NULL || frame == NULL) return DEVMGR_ERROR_INVALID;
    memset(frame, 0, sizeof(*frame));
    switch (upgrade->state) {
    case DEVMGR_UPGRADE_ENTER_BOOTLOADER:
        frame->type = DEVMGR_MSG_ENTER_BOOTLOADER;
        break;
    case DEVMGR_UPGRADE_BEGIN:
        frame->type = DEVMGR_MSG_FW_BEGIN;
        devmgr_put_le32(frame->payload, (uint32_t)upgrade->image_size);
        devmgr_put_le32(frame->payload + 4U, upgrade->image_crc);
        devmgr_put_le16(frame->payload + 8U, upgrade->chunk_size);
        version_length = strlen(upgrade->version) + 1U;
        memcpy(frame->payload + 10U, upgrade->version, version_length);
        frame->payload_length = (uint16_t)(10U + version_length);
        break;
    case DEVMGR_UPGRADE_TRANSFER:
        if (upgrade->offset >= upgrade->image_size) return DEVMGR_ERROR_STATE;
        frame->type = DEVMGR_MSG_FW_DATA;
        devmgr_put_le32(frame->payload, upgrade->session_id);
        devmgr_put_le32(frame->payload + 4U, upgrade->offset);
        remaining = upgrade->image_size - upgrade->offset;
        chunk = remaining < upgrade->chunk_size ? remaining : upgrade->chunk_size;
        memcpy(frame->payload + 8U, upgrade->image_data + upgrade->offset, chunk);
        frame->payload_length = (uint16_t)(8U + chunk);
        upgrade->last_chunk_end = upgrade->offset + (uint32_t)chunk;
        ++upgrade->chunks_sent;
        break;
    case DEVMGR_UPGRADE_RECOVER:
        if (upgrade->session_id == 0U) return DEVMGR_ERROR_STATE;
        frame->type = DEVMGR_MSG_FW_STATUS;
        devmgr_put_le32(frame->payload, upgrade->session_id);
        frame->payload_length = 4U;
        break;
    case DEVMGR_UPGRADE_END:
    case DEVMGR_UPGRADE_VERIFY:
    case DEVMGR_UPGRADE_ACTIVATE:
        frame->type = upgrade->state == DEVMGR_UPGRADE_END       ? DEVMGR_MSG_FW_END
                      : upgrade->state == DEVMGR_UPGRADE_VERIFY ? DEVMGR_MSG_FW_VERIFY
                                                               : DEVMGR_MSG_FW_ACTIVATE;
        devmgr_put_le32(frame->payload, upgrade->session_id);
        frame->payload_length = 4U;
        break;
    case DEVMGR_UPGRADE_REBOOT:
        frame->type = DEVMGR_MSG_REBOOT;
        frame->payload[0] = 0U;
        frame->payload_length = 1U;
        break;
    default: return DEVMGR_ERROR_STATE;
    }
    frame->flags = DEVMGR_FRAME_ACK_REQUIRED;
    return DEVMGR_OK;
}

int devmgr_upgrade_accept_response(struct devmgr_upgrade *upgrade, uint8_t request_type,
                                   const struct devmgr_frame *response)
{
    if (upgrade == NULL || response == NULL) return DEVMGR_ERROR_INVALID;
    if (response->type == DEVMGR_MSG_NACK || response->type == DEVMGR_MSG_ERROR) {
        upgrade->state = DEVMGR_UPGRADE_ERROR;
        return DEVMGR_ERROR_PROTOCOL;
    }
    switch (upgrade->state) {
    case DEVMGR_UPGRADE_ENTER_BOOTLOADER:
        if (request_type != DEVMGR_MSG_ENTER_BOOTLOADER) break;
        upgrade->state = DEVMGR_UPGRADE_BEGIN;
        return DEVMGR_OK;
    case DEVMGR_UPGRADE_BEGIN:
        if (request_type != DEVMGR_MSG_FW_BEGIN || response->payload_length != 8U) break;
        upgrade->session_id = devmgr_get_le32(response->payload);
        upgrade->offset = devmgr_get_le32(response->payload + 4U);
        if (upgrade->session_id == 0U || upgrade->offset > upgrade->image_size) break;
        upgrade->state = upgrade->offset == upgrade->image_size ? DEVMGR_UPGRADE_END
                                                                : DEVMGR_UPGRADE_TRANSFER;
        return DEVMGR_OK;
    case DEVMGR_UPGRADE_TRANSFER:
        if (request_type != DEVMGR_MSG_FW_DATA || response->payload_length != 4U) break;
        upgrade->offset = devmgr_get_le32(response->payload);
        if (upgrade->offset != upgrade->last_chunk_end || upgrade->offset > upgrade->image_size) break;
        upgrade->state = upgrade->offset == upgrade->image_size ? DEVMGR_UPGRADE_END
                                                                : DEVMGR_UPGRADE_TRANSFER;
        return DEVMGR_OK;
    case DEVMGR_UPGRADE_RECOVER:
        if (request_type != DEVMGR_MSG_FW_STATUS || response->payload_length != 8U ||
            devmgr_get_le32(response->payload) != upgrade->session_id) break;
        upgrade->offset = devmgr_get_le32(response->payload + 4U);
        if (upgrade->offset > upgrade->image_size) break;
        upgrade->state = upgrade->offset == upgrade->image_size ? DEVMGR_UPGRADE_END
                                                                : DEVMGR_UPGRADE_TRANSFER;
        return DEVMGR_OK;
    case DEVMGR_UPGRADE_END:
        if (request_type != DEVMGR_MSG_FW_END) break;
        upgrade->state = DEVMGR_UPGRADE_VERIFY;
        return DEVMGR_OK;
    case DEVMGR_UPGRADE_VERIFY:
        if (request_type != DEVMGR_MSG_FW_VERIFY || response->payload_length != 4U ||
            devmgr_get_le32(response->payload) != upgrade->image_crc) break;
        upgrade->state = DEVMGR_UPGRADE_ACTIVATE;
        return DEVMGR_OK;
    case DEVMGR_UPGRADE_ACTIVATE:
        if (request_type != DEVMGR_MSG_FW_ACTIVATE) break;
        upgrade->state = DEVMGR_UPGRADE_REBOOT;
        return DEVMGR_OK;
    case DEVMGR_UPGRADE_REBOOT:
        if (request_type != DEVMGR_MSG_REBOOT) break;
        upgrade->state = DEVMGR_UPGRADE_COMPLETE;
        return DEVMGR_OK;
    default: break;
    }
    upgrade->state = DEVMGR_UPGRADE_ERROR;
    return DEVMGR_ERROR_PROTOCOL;
}

bool devmgr_upgrade_active(const struct devmgr_upgrade *upgrade)
{
    return upgrade != NULL && upgrade->state != DEVMGR_UPGRADE_IDLE &&
           upgrade->state != DEVMGR_UPGRADE_COMPLETE && upgrade->state != DEVMGR_UPGRADE_ERROR;
}

int devmgr_upgrade_begin_recovery(struct devmgr_upgrade *upgrade)
{
    if (upgrade == NULL || upgrade->session_id == 0U ||
        (upgrade->state != DEVMGR_UPGRADE_TRANSFER && upgrade->state != DEVMGR_UPGRADE_RECOVER))
        return DEVMGR_ERROR_STATE;
    if (upgrade->recovery_attempts >= 3U) {
        upgrade->state = DEVMGR_UPGRADE_ERROR;
        return DEVMGR_ERROR_TIMEOUT;
    }
    ++upgrade->recovery_attempts;
    upgrade->state = DEVMGR_UPGRADE_RECOVER;
    return DEVMGR_OK;
}

const char *devmgr_upgrade_state_string(enum devmgr_upgrade_state state)
{
    switch (state) {
    case DEVMGR_UPGRADE_IDLE: return "IDLE";
    case DEVMGR_UPGRADE_VALIDATING: return "VALIDATING";
    case DEVMGR_UPGRADE_ENTER_BOOTLOADER: return "ENTER_BOOTLOADER";
    case DEVMGR_UPGRADE_BEGIN: return "BEGIN";
    case DEVMGR_UPGRADE_TRANSFER: return "TRANSFER";
    case DEVMGR_UPGRADE_RECOVER: return "RECOVER";
    case DEVMGR_UPGRADE_END: return "END";
    case DEVMGR_UPGRADE_VERIFY: return "VERIFY";
    case DEVMGR_UPGRADE_ACTIVATE: return "ACTIVATE";
    case DEVMGR_UPGRADE_REBOOT: return "REBOOT";
    case DEVMGR_UPGRADE_COMPLETE: return "COMPLETE";
    case DEVMGR_UPGRADE_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}
