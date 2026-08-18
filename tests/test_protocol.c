#include "test.h"

#include "devmgr/error.h"
#include "devmgr/parser.h"
#include "devmgr/protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct collector {
    struct devmgr_frame frames[8];
    size_t count;
};

static int collect_frame(const struct devmgr_frame *frame, void *context)
{
    struct collector *collector = context;
    if (collector->count >= sizeof(collector->frames) / sizeof(collector->frames[0])) {
        return DEVMGR_ERROR_LIMIT;
    }
    collector->frames[collector->count++] = *frame;
    return DEVMGR_OK;
}

static struct devmgr_frame sample_frame(uint32_t sequence, uint8_t marker)
{
    struct devmgr_frame frame = {0};
    frame.type = DEVMGR_MSG_PING;
    frame.flags = DEVMGR_FRAME_ACK_REQUIRED;
    frame.sequence = sequence;
    frame.payload_length = 3U;
    frame.payload[0] = marker;
    frame.payload[1] = (uint8_t)(marker + 1U);
    frame.payload[2] = (uint8_t)(marker + 2U);
    return frame;
}

static int test_codec_roundtrip(void)
{
    struct devmgr_frame input = sample_frame(UINT32_C(0x78563412), 10U);
    struct devmgr_frame output = {0};
    uint8_t encoded[DEVMGR_PROTOCOL_MAX_FRAME];
    size_t length = 0U;

    TEST_CHECK(devmgr_frame_encode(&input, encoded, sizeof(encoded), &length) == DEVMGR_OK);
    TEST_CHECK(length == DEVMGR_PROTOCOL_HEADER_SIZE + 3U + DEVMGR_PROTOCOL_CRC_SIZE);
    TEST_CHECK(encoded[0] == 0x44U && encoded[1] == 0x4DU);
    TEST_CHECK(encoded[6] == 0x12U && encoded[9] == 0x78U);
    TEST_CHECK(devmgr_frame_decode(encoded, length, &output) == DEVMGR_OK);
    TEST_CHECK(output.type == input.type && output.flags == input.flags);
    TEST_CHECK(output.sequence == input.sequence && output.payload_length == input.payload_length);
    TEST_CHECK(memcmp(output.payload, input.payload, input.payload_length) == 0);
    TEST_CHECK(devmgr_frame_encode(&input, encoded, 4U, &length) == DEVMGR_ERROR_OVERFLOW);
    return 0;
}

static int test_parser_fragmentation(void)
{
    struct devmgr_parser parser;
    struct collector collector = {0};
    struct devmgr_frame input = sample_frame(7U, 20U);
    uint8_t encoded[DEVMGR_PROTOCOL_MAX_FRAME];
    size_t length = 0U;
    size_t emitted = 0U;

    TEST_CHECK(devmgr_parser_init(&parser) == DEVMGR_OK);
    TEST_CHECK(devmgr_frame_encode(&input, encoded, sizeof(encoded), &length) == DEVMGR_OK);
    for (size_t index = 0; index < length; ++index) {
        TEST_CHECK(devmgr_parser_feed(&parser, encoded + index, 1U, collect_frame, &collector,
                                      &emitted) == DEVMGR_OK);
    }
    TEST_CHECK(collector.count == 1U && collector.frames[0].sequence == 7U);
    TEST_CHECK(devmgr_parser_get_stats(&parser)->frames_decoded == 1U);
    return 0;
}

static int test_parser_coalesced(void)
{
    struct devmgr_parser parser;
    struct collector collector = {0};
    struct devmgr_frame first = sample_frame(1U, 1U);
    struct devmgr_frame second = sample_frame(2U, 2U);
    uint8_t stream[DEVMGR_PROTOCOL_MAX_FRAME * 2U];
    size_t first_length = 0U;
    size_t second_length = 0U;
    size_t emitted = 0U;

    TEST_CHECK(devmgr_parser_init(&parser) == DEVMGR_OK);
    TEST_CHECK(devmgr_frame_encode(&first, stream, sizeof(stream), &first_length) == DEVMGR_OK);
    TEST_CHECK(devmgr_frame_encode(&second, stream + first_length, sizeof(stream) - first_length,
                                   &second_length) == DEVMGR_OK);
    TEST_CHECK(devmgr_parser_feed(&parser, stream, first_length + second_length, collect_frame,
                                  &collector, &emitted) == DEVMGR_OK);
    TEST_CHECK(emitted == 2U && collector.count == 2U);
    TEST_CHECK(collector.frames[0].sequence == 1U && collector.frames[1].sequence == 2U);
    return 0;
}

static int test_parser_recovery(void)
{
    struct devmgr_parser parser;
    struct collector collector = {0};
    struct devmgr_frame bad = sample_frame(3U, 3U);
    struct devmgr_frame good = sample_frame(4U, 4U);
    uint8_t stream[DEVMGR_PROTOCOL_MAX_FRAME * 2U + 5U] = {0xAAU, 0x44U, 0x00U, 0xFFU, 0x01U};
    size_t bad_length = 0U;
    size_t good_length = 0U;
    size_t emitted = 0U;

    TEST_CHECK(devmgr_parser_init(&parser) == DEVMGR_OK);
    TEST_CHECK(devmgr_frame_encode(&bad, stream + 5U, DEVMGR_PROTOCOL_MAX_FRAME, &bad_length) ==
               DEVMGR_OK);
    stream[5U + DEVMGR_PROTOCOL_HEADER_SIZE] ^= 0x80U;
    TEST_CHECK(devmgr_frame_encode(&good, stream + 5U + bad_length, DEVMGR_PROTOCOL_MAX_FRAME,
                                   &good_length) == DEVMGR_OK);
    TEST_CHECK(devmgr_parser_feed(&parser, stream, 5U + bad_length + good_length, collect_frame,
                                  &collector, &emitted) == DEVMGR_OK);
    TEST_CHECK(collector.count == 1U && collector.frames[0].sequence == 4U);
    TEST_CHECK(devmgr_parser_get_stats(&parser)->crc_errors == 1U);
    TEST_CHECK(devmgr_parser_get_stats(&parser)->discarded_bytes >= 6U);
    return 0;
}

static int test_invalid_length_resync(void)
{
    struct devmgr_parser parser;
    struct collector collector = {0};
    struct devmgr_frame good = sample_frame(9U, 9U);
    uint8_t stream[DEVMGR_PROTOCOL_MAX_FRAME + DEVMGR_PROTOCOL_HEADER_SIZE] = {0};
    size_t good_length = 0U;
    size_t emitted = 0U;

    devmgr_put_le16(stream, DEVMGR_PROTOCOL_MAGIC);
    stream[2] = DEVMGR_PROTOCOL_VERSION;
    devmgr_put_le16(stream + 10U, UINT16_MAX);
    TEST_CHECK(devmgr_frame_encode(&good, stream + DEVMGR_PROTOCOL_HEADER_SIZE,
                                   DEVMGR_PROTOCOL_MAX_FRAME, &good_length) == DEVMGR_OK);
    TEST_CHECK(devmgr_parser_init(&parser) == DEVMGR_OK);
    TEST_CHECK(devmgr_parser_feed(&parser, stream, DEVMGR_PROTOCOL_HEADER_SIZE + good_length,
                                  collect_frame, &collector, &emitted) == DEVMGR_OK);
    TEST_CHECK(collector.count == 1U && collector.frames[0].sequence == 9U);
    TEST_CHECK(devmgr_parser_get_stats(&parser)->length_errors == 1U);
    return 0;
}

int main(void)
{
    int failed = 0;
    TEST_RUN(test_codec_roundtrip);
    TEST_RUN(test_parser_fragmentation);
    TEST_RUN(test_parser_coalesced);
    TEST_RUN(test_parser_recovery);
    TEST_RUN(test_invalid_length_resync);
    (void)printf("protocol tests: %s\n", failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

