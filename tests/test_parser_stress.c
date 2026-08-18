#include "test.h"

#include "devmgr/error.h"
#include "devmgr/parser.h"
#include "devmgr/protocol.h"

#include <stdint.h>
#include <string.h>

struct stress_context {
    uint32_t next_sequence;
    size_t frames;
};

static int verify_frame(const struct devmgr_frame *frame, void *opaque)
{
    struct stress_context *context = opaque;
    if (frame->sequence != context->next_sequence || frame->payload_length != 17U)
        return DEVMGR_ERROR_PROTOCOL;
    ++context->next_sequence;
    ++context->frames;
    return DEVMGR_OK;
}

static uint32_t random_next(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static int feed_sliced(struct devmgr_parser *parser, const uint8_t *data, size_t length,
                       struct stress_context *context, uint32_t *random_state)
{
    size_t offset = 0U;
    while (offset < length) {
        size_t chunk = 1U + random_next(random_state) % 23U;
        if (chunk > length - offset) chunk = length - offset;
        size_t emitted = 0U;
        int result = devmgr_parser_feed(parser, data + offset, chunk, verify_frame, context, &emitted);
        if (result != DEVMGR_OK) return result;
        offset += chunk;
    }
    return DEVMGR_OK;
}

static int test_parser_stress(void)
{
    struct devmgr_parser parser;
    struct stress_context context = {0};
    uint32_t random_state = 7U;
    uint8_t encoded[DEVMGR_PROTOCOL_MAX_FRAME];
    const uint8_t garbage[] = {0x00U, 0x44U, 0x7FU, 0x4DU, 0xAAU};
    TEST_CHECK(devmgr_parser_init(&parser) == DEVMGR_OK);
    for (uint32_t sequence = 0U; sequence < 2000U; ++sequence) {
        struct devmgr_frame frame = {.type = DEVMGR_MSG_PING, .sequence = sequence,
                                     .payload_length = 17U};
        for (size_t index = 0U; index < frame.payload_length; ++index)
            frame.payload[index] = (uint8_t)random_next(&random_state);
        size_t length = 0U;
        if (sequence % 11U == 0U) {
            size_t emitted = 0U;
            TEST_CHECK(devmgr_parser_feed(&parser, garbage, sizeof(garbage), verify_frame,
                                          &context, &emitted) == DEVMGR_OK);
        }
        TEST_CHECK(devmgr_frame_encode(&frame, encoded, sizeof(encoded), &length) == DEVMGR_OK);
        TEST_CHECK(feed_sliced(&parser, encoded, length, &context, &random_state) == DEVMGR_OK);
    }
    TEST_CHECK(context.frames == 2000U && context.next_sequence == 2000U);
    TEST_CHECK(devmgr_parser_get_stats(&parser)->discarded_bytes > 0U);
    return 0;
}

static int test_parser_random_smoke(void)
{
    struct devmgr_parser parser;
    struct stress_context context = {0};
    uint32_t random_state = 99U;
    uint8_t input[97];
    TEST_CHECK(devmgr_parser_init(&parser) == DEVMGR_OK);
    for (unsigned iteration = 0U; iteration < 10000U; ++iteration) {
        size_t length = random_next(&random_state) % sizeof(input);
        for (size_t index = 0U; index < length; ++index)
            input[index] = (uint8_t)random_next(&random_state);
        size_t emitted = 0U;
        int result = devmgr_parser_feed(&parser, input, length, verify_frame, &context, &emitted);
        TEST_CHECK(result == DEVMGR_OK || result == DEVMGR_ERROR_PROTOCOL);
        if (result == DEVMGR_ERROR_PROTOCOL) devmgr_parser_reset(&parser);
    }
    return 0;
}

int main(void)
{
    int failed = 0;
    TEST_RUN(test_parser_stress);
    TEST_RUN(test_parser_random_smoke);
    return failed == 0 ? 0 : 1;
}

