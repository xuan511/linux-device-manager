#include "test.h"

#include "devmgr/error.h"
#include "devmgr/ring_buffer.h"

#include <stdint.h>
#include <string.h>

int test_ring_boundaries(void)
{
    struct devmgr_ring_buffer ring;
    uint8_t storage[4];
    uint8_t output[4];
    static const uint8_t input[] = {1U, 2U, 3U, 4U, 5U};

    TEST_CHECK(devmgr_ring_init(NULL, storage, sizeof(storage)) == DEVMGR_ERROR_INVALID);
    TEST_CHECK(devmgr_ring_init(&ring, storage, 0U) == DEVMGR_ERROR_INVALID);
    TEST_CHECK(devmgr_ring_init(&ring, storage, sizeof(storage)) == DEVMGR_OK);
    TEST_CHECK(devmgr_ring_empty(&ring));
    TEST_CHECK(devmgr_ring_read(&ring, output, sizeof(output)) == 0U);
    TEST_CHECK(devmgr_ring_write(&ring, input, sizeof(input)) == sizeof(storage));
    TEST_CHECK(devmgr_ring_full(&ring));
    TEST_CHECK(devmgr_ring_write(&ring, input, 1U) == 0U);
    TEST_CHECK(devmgr_ring_read(&ring, output, sizeof(output)) == sizeof(output));
    TEST_CHECK(memcmp(output, input, sizeof(output)) == 0);
    TEST_CHECK(devmgr_ring_empty(&ring));
    return 0;
}

int test_ring_wraparound(void)
{
    struct devmgr_ring_buffer ring;
    uint8_t storage[5];
    uint8_t output[5];
    static const uint8_t first[] = {1U, 2U, 3U, 4U};
    static const uint8_t second[] = {5U, 6U, 7U};
    static const uint8_t expected[] = {3U, 4U, 5U, 6U, 7U};

    TEST_CHECK(devmgr_ring_init(&ring, storage, sizeof(storage)) == DEVMGR_OK);
    TEST_CHECK(devmgr_ring_write(&ring, first, sizeof(first)) == sizeof(first));
    TEST_CHECK(devmgr_ring_discard(&ring, 2U) == 2U);
    TEST_CHECK(devmgr_ring_write(&ring, second, sizeof(second)) == sizeof(second));
    TEST_CHECK(devmgr_ring_peek(&ring, 0U, output, sizeof(output)) == sizeof(output));
    TEST_CHECK(memcmp(output, expected, sizeof(expected)) == 0);
    TEST_CHECK(devmgr_ring_size(&ring) == sizeof(expected));
    return 0;
}

int test_ring_partial(void)
{
    struct devmgr_ring_buffer ring;
    uint8_t storage[4];
    uint8_t output[8] = {0U};
    static const uint8_t input[] = {8U, 9U, 10U};

    TEST_CHECK(devmgr_ring_init(&ring, storage, sizeof(storage)) == DEVMGR_OK);
    TEST_CHECK(devmgr_ring_write(&ring, input, sizeof(input)) == sizeof(input));
    TEST_CHECK(devmgr_ring_peek(&ring, 1U, output, sizeof(output)) == 2U);
    TEST_CHECK(output[0] == 9U && output[1] == 10U);
    TEST_CHECK(devmgr_ring_discard(&ring, 99U) == sizeof(input));
    TEST_CHECK(devmgr_ring_empty(&ring));
    devmgr_ring_reset(&ring);
    TEST_CHECK(devmgr_ring_space(&ring) == sizeof(storage));
    return 0;
}

