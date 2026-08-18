#include "test.h"

#include <stddef.h>
#include <stdio.h>

int main(void)
{
    static const struct test_case tests[] = {
        {"crc32 known vectors", test_crc32_vectors},
        {"crc32 incremental", test_crc32_incremental},
        {"ring boundaries", test_ring_boundaries},
        {"ring wraparound", test_ring_wraparound},
        {"ring partial operations", test_ring_partial},
    };
    int failed = 0;

    for (size_t index = 0; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        int result = tests[index].run();
        (void)printf("[%s] %s\n", result == 0 ? "PASS" : "FAIL", tests[index].name);
        failed += result != 0;
    }
    (void)printf("%zu tests, %d failures\n", sizeof(tests) / sizeof(tests[0]), failed);
    return failed == 0 ? 0 : 1;
}
