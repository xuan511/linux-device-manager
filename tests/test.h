#ifndef DEVMGR_TEST_H
#define DEVMGR_TEST_H

#include <stdio.h>

struct test_case {
    const char *name;
    int (*run)(void);
};

int test_crc32_vectors(void);
int test_crc32_incremental(void);
int test_ring_boundaries(void);
int test_ring_wraparound(void);
int test_ring_partial(void);

#define TEST_CHECK(expression)                                                                  \
    do {                                                                                        \
        if (!(expression)) {                                                                    \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expression); \
            return 1;                                                                           \
        }                                                                                       \
    } while (0)

#define TEST_RUN(name)                                                                          \
    do {                                                                                        \
        int result = (name)();                                                                  \
        (void)printf("[%s] %s\n", result == 0 ? "PASS" : "FAIL", #name);                       \
        failed += result != 0;                                                                  \
    } while (0)


#endif
