#include "test.h"

#include "devmgr/error.h"
#include "devmgr/ipc.h"
#include "devmgr/protocol.h"

#include <string.h>

static int test_ipc_roundtrip(void)
{
    struct devmgr_ipc_request request = {.command = DEVMGR_MSG_PING, .payload_length = 3U,
                                         .payload = {1U, 2U, 3U}};
    struct devmgr_ipc_request decoded_request = {0};
    struct devmgr_ipc_response response = {.command = DEVMGR_MSG_PING,
                                           .status = DEVMGR_ERROR_TIMEOUT,
                                           .payload_length = 2U,
                                           .payload = {8U, 9U}};
    struct devmgr_ipc_response decoded_response = {0};
    uint8_t buffer[DEVMGR_IPC_RESPONSE_HEADER_SIZE + DEVMGR_IPC_MAX_PAYLOAD];
    size_t length = 0U;

    TEST_CHECK(devmgr_ipc_encode_request(&request, buffer, sizeof(buffer), &length) == DEVMGR_OK);
    TEST_CHECK(devmgr_ipc_decode_request(buffer, length, &decoded_request) == DEVMGR_OK);
    TEST_CHECK(decoded_request.command == request.command && decoded_request.payload_length == 3U);
    TEST_CHECK(memcmp(decoded_request.payload, request.payload, 3U) == 0);
    TEST_CHECK(devmgr_ipc_encode_response(&response, buffer, sizeof(buffer), &length) == DEVMGR_OK);
    TEST_CHECK(devmgr_ipc_decode_response(buffer, length, &decoded_response) == DEVMGR_OK);
    TEST_CHECK(decoded_response.status == DEVMGR_ERROR_TIMEOUT);
    TEST_CHECK(memcmp(decoded_response.payload, response.payload, 2U) == 0);
    buffer[0] = 0U;
    TEST_CHECK(devmgr_ipc_decode_response(buffer, length, &decoded_response) ==
               DEVMGR_ERROR_PROTOCOL);
    return 0;
}

int main(void)
{
    int failed = 0;
    TEST_RUN(test_ipc_roundtrip);
    return failed == 0 ? 0 : 1;
}

