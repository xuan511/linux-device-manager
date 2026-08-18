#ifndef DEVMGR_IPC_H
#define DEVMGR_IPC_H

#include <stddef.h>
#include <stdint.h>

#define DEVMGR_IPC_MAGIC UINT16_C(0x5049)
#define DEVMGR_IPC_VERSION UINT8_C(1)
#define DEVMGR_IPC_REQUEST_HEADER_SIZE 8U
#define DEVMGR_IPC_RESPONSE_HEADER_SIZE 12U
#define DEVMGR_IPC_MAX_PAYLOAD 4096U
#define DEVMGR_IPC_GET_TELEMETRY UINT8_C(0xF0)
#define DEVMGR_IPC_UPGRADE UINT8_C(0xF1)
#define DEVMGR_IPC_UPGRADE_STATUS UINT8_C(0xF2)

struct devmgr_ipc_request {
    uint8_t command;
    uint32_t payload_length;
    uint8_t payload[DEVMGR_IPC_MAX_PAYLOAD];
};

struct devmgr_ipc_response {
    uint8_t command;
    int32_t status;
    uint32_t payload_length;
    uint8_t payload[DEVMGR_IPC_MAX_PAYLOAD];
};

int devmgr_ipc_encode_request(const struct devmgr_ipc_request *request, uint8_t *output,
                              size_t capacity, size_t *encoded_length);
int devmgr_ipc_decode_request(const uint8_t *data, size_t length,
                              struct devmgr_ipc_request *request);
int devmgr_ipc_encode_response(const struct devmgr_ipc_response *response, uint8_t *output,
                               size_t capacity, size_t *encoded_length);
int devmgr_ipc_decode_response(const uint8_t *data, size_t length,
                               struct devmgr_ipc_response *response);

#endif
