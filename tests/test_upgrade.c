#include "test.h"

#include "devmgr/crc32.h"
#include "devmgr/error.h"
#include "devmgr/upgrade.h"

#include <stdint.h>
#include <string.h>

static void response_for(const struct devmgr_upgrade *upgrade, const struct devmgr_frame *request,
                         struct devmgr_frame *response)
{
    memset(response, 0, sizeof(*response));
    response->type = request->type;
    response->flags = DEVMGR_FRAME_RESPONSE;
    if (request->type == DEVMGR_MSG_FW_BEGIN) {
        devmgr_put_le32(response->payload, 77U);
        devmgr_put_le32(response->payload + 4U, 0U);
        response->payload_length = 8U;
    } else if (request->type == DEVMGR_MSG_FW_DATA) {
        devmgr_put_le32(response->payload, upgrade->last_chunk_end);
        response->payload_length = 4U;
    } else if (request->type == DEVMGR_MSG_FW_VERIFY) {
        devmgr_put_le32(response->payload, upgrade->image_crc);
        response->payload_length = 4U;
    }
}

static int test_complete_upgrade(void)
{
    uint8_t image[2500];
    struct devmgr_upgrade upgrade;
    struct devmgr_frame request;
    struct devmgr_frame response;
    for (size_t index = 0U; index < sizeof(image); ++index) image[index] = (uint8_t)index;
    uint32_t crc = devmgr_crc32(image, sizeof(image));
    TEST_CHECK(devmgr_upgrade_start(&upgrade, image, sizeof(image), crc, "1.1.0", 1024U) == DEVMGR_OK);
    unsigned steps = 0U;
    while (devmgr_upgrade_active(&upgrade)) {
        TEST_CHECK(devmgr_upgrade_build_request(&upgrade, &request) == DEVMGR_OK);
        response_for(&upgrade, &request, &response);
        TEST_CHECK(devmgr_upgrade_accept_response(&upgrade, request.type, &response) == DEVMGR_OK);
        TEST_CHECK(++steps < 20U);
    }
    TEST_CHECK(upgrade.state == DEVMGR_UPGRADE_COMPLETE);
    TEST_CHECK(upgrade.offset == sizeof(image));
    TEST_CHECK(upgrade.chunks_sent == 3U);
    return 0;
}

static int test_upgrade_rejects_bad_offset(void)
{
    uint8_t image[16] = {0U};
    struct devmgr_upgrade upgrade;
    struct devmgr_frame request;
    struct devmgr_frame response = {.type = DEVMGR_MSG_FW_BEGIN,
                                    .flags = DEVMGR_FRAME_RESPONSE,
                                    .payload_length = 8U};
    TEST_CHECK(devmgr_upgrade_start(&upgrade, image, sizeof(image), 1U, "2.0.0", 8U) == DEVMGR_OK);
    TEST_CHECK(devmgr_upgrade_build_request(&upgrade, &request) == DEVMGR_OK);
    TEST_CHECK(devmgr_upgrade_accept_response(&upgrade, request.type, &response) == DEVMGR_OK);
    TEST_CHECK(devmgr_upgrade_build_request(&upgrade, &request) == DEVMGR_OK);
    devmgr_put_le32(response.payload, 9U);
    devmgr_put_le32(response.payload + 4U, 99U);
    TEST_CHECK(devmgr_upgrade_accept_response(&upgrade, request.type, &response) ==
               DEVMGR_ERROR_PROTOCOL);
    TEST_CHECK(upgrade.state == DEVMGR_UPGRADE_ERROR);
    return 0;
}

static int test_upgrade_resume_status(void)
{
    uint8_t image[32] = {0U};
    struct devmgr_upgrade upgrade;
    struct devmgr_frame request;
    struct devmgr_frame response = {.flags = DEVMGR_FRAME_RESPONSE};
    TEST_CHECK(devmgr_upgrade_start(&upgrade, image, sizeof(image), 1U, "3.0.0", 8U) == DEVMGR_OK);
    TEST_CHECK(devmgr_upgrade_build_request(&upgrade, &request) == DEVMGR_OK);
    TEST_CHECK(devmgr_upgrade_accept_response(&upgrade, request.type, &response) == DEVMGR_OK);
    TEST_CHECK(devmgr_upgrade_build_request(&upgrade, &request) == DEVMGR_OK);
    response.payload_length = 8U;
    devmgr_put_le32(response.payload, 42U);
    devmgr_put_le32(response.payload + 4U, 0U);
    TEST_CHECK(devmgr_upgrade_accept_response(&upgrade, request.type, &response) == DEVMGR_OK);
    TEST_CHECK(devmgr_upgrade_begin_recovery(&upgrade) == DEVMGR_OK);
    TEST_CHECK(devmgr_upgrade_build_request(&upgrade, &request) == DEVMGR_OK);
    TEST_CHECK(request.type == DEVMGR_MSG_FW_STATUS);
    devmgr_put_le32(response.payload, 42U);
    devmgr_put_le32(response.payload + 4U, 16U);
    TEST_CHECK(devmgr_upgrade_accept_response(&upgrade, request.type, &response) == DEVMGR_OK);
    TEST_CHECK(upgrade.state == DEVMGR_UPGRADE_TRANSFER && upgrade.offset == 16U);
    return 0;
}

int main(void)
{
    int failed = 0;
    TEST_RUN(test_complete_upgrade);
    TEST_RUN(test_upgrade_rejects_bad_offset);
    TEST_RUN(test_upgrade_resume_status);
    return failed == 0 ? 0 : 1;
}
