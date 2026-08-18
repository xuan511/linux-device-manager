#include "test.h"

#include "devmgr/crc32.h"

#include <stdint.h>
#include <string.h>

int test_crc32_vectors(void)
{
    static const char check[] = "123456789";
    TEST_CHECK(devmgr_crc32(NULL, 0U) == UINT32_C(0x00000000));
    TEST_CHECK(devmgr_crc32(check, strlen(check)) == UINT32_C(0xCBF43926));
    return 0;
}

int test_crc32_incremental(void)
{
    static const char first[] = "1234";
    static const char second[] = "56789";
    uint32_t state = devmgr_crc32_update(DEVMGR_CRC32_INIT, first, strlen(first));
    state = devmgr_crc32_update(state, second, strlen(second));
    TEST_CHECK(devmgr_crc32_finish(state) == UINT32_C(0xCBF43926));
    return 0;
}

