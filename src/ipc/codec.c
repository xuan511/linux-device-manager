#include "devmgr/ipc.h"

#include "devmgr/error.h"
#include "devmgr/protocol.h"

#include <string.h>

static int validate_header(const uint8_t *data, size_t length, size_t header_size)
{
    if (data == NULL || length < header_size) {
        return DEVMGR_ERROR_INVALID;
    }
    if (devmgr_get_le16(data) != DEVMGR_IPC_MAGIC || data[2] != DEVMGR_IPC_VERSION) {
        return DEVMGR_ERROR_PROTOCOL;
    }
    return DEVMGR_OK;
}

int devmgr_ipc_encode_request(const struct devmgr_ipc_request *request, uint8_t *output,
                              size_t capacity, size_t *encoded_length)
{
    size_t total;
    if (request == NULL || output == NULL || encoded_length == NULL ||
        request->payload_length > DEVMGR_IPC_MAX_PAYLOAD) {
        return DEVMGR_ERROR_INVALID;
    }
    total = DEVMGR_IPC_REQUEST_HEADER_SIZE + request->payload_length;
    if (capacity < total) {
        return DEVMGR_ERROR_OVERFLOW;
    }
    devmgr_put_le16(output, DEVMGR_IPC_MAGIC);
    output[2] = DEVMGR_IPC_VERSION;
    output[3] = request->command;
    devmgr_put_le32(output + 4U, request->payload_length);
    memcpy(output + DEVMGR_IPC_REQUEST_HEADER_SIZE, request->payload, request->payload_length);
    *encoded_length = total;
    return DEVMGR_OK;
}

int devmgr_ipc_decode_request(const uint8_t *data, size_t length, struct devmgr_ipc_request *request)
{
    uint32_t payload_length;
    int result = validate_header(data, length, DEVMGR_IPC_REQUEST_HEADER_SIZE);
    if (result != DEVMGR_OK || request == NULL) {
        return result != DEVMGR_OK ? result : DEVMGR_ERROR_INVALID;
    }
    payload_length = devmgr_get_le32(data + 4U);
    if (payload_length > DEVMGR_IPC_MAX_PAYLOAD) {
        return DEVMGR_ERROR_LIMIT;
    }
    if (length != DEVMGR_IPC_REQUEST_HEADER_SIZE + payload_length) {
        return DEVMGR_ERROR_INVALID;
    }
    request->command = data[3];
    request->payload_length = payload_length;
    memcpy(request->payload, data + DEVMGR_IPC_REQUEST_HEADER_SIZE, payload_length);
    return DEVMGR_OK;
}

int devmgr_ipc_encode_response(const struct devmgr_ipc_response *response, uint8_t *output,
                               size_t capacity, size_t *encoded_length)
{
    size_t total;
    if (response == NULL || output == NULL || encoded_length == NULL ||
        response->payload_length > DEVMGR_IPC_MAX_PAYLOAD) {
        return DEVMGR_ERROR_INVALID;
    }
    total = DEVMGR_IPC_RESPONSE_HEADER_SIZE + response->payload_length;
    if (capacity < total) {
        return DEVMGR_ERROR_OVERFLOW;
    }
    devmgr_put_le16(output, DEVMGR_IPC_MAGIC);
    output[2] = DEVMGR_IPC_VERSION;
    output[3] = response->command;
    devmgr_put_le32(output + 4U, (uint32_t)response->status);
    devmgr_put_le32(output + 8U, response->payload_length);
    memcpy(output + DEVMGR_IPC_RESPONSE_HEADER_SIZE, response->payload, response->payload_length);
    *encoded_length = total;
    return DEVMGR_OK;
}

int devmgr_ipc_decode_response(const uint8_t *data, size_t length,
                               struct devmgr_ipc_response *response)
{
    uint32_t payload_length;
    int result = validate_header(data, length, DEVMGR_IPC_RESPONSE_HEADER_SIZE);
    if (result != DEVMGR_OK || response == NULL) {
        return result != DEVMGR_OK ? result : DEVMGR_ERROR_INVALID;
    }
    payload_length = devmgr_get_le32(data + 8U);
    if (payload_length > DEVMGR_IPC_MAX_PAYLOAD) {
        return DEVMGR_ERROR_LIMIT;
    }
    if (length != DEVMGR_IPC_RESPONSE_HEADER_SIZE + payload_length) {
        return DEVMGR_ERROR_INVALID;
    }
    response->command = data[3];
    response->status = (int32_t)devmgr_get_le32(data + 4U);
    response->payload_length = payload_length;
    memcpy(response->payload, data + DEVMGR_IPC_RESPONSE_HEADER_SIZE, payload_length);
    return DEVMGR_OK;
}

