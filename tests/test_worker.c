#define _GNU_SOURCE

#include "test.h"

#include "devmgr/crc32.h"
#include "devmgr/error.h"
#include "devmgr/worker.h"

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int test_worker_validation(void)
{
    char path[] = "/tmp/devmgr-worker-XXXXXX";
    static const uint8_t contents[] = {1U, 2U, 3U, 4U, 5U};
    int fd = mkstemp(path);
    TEST_CHECK(fd >= 0);
    TEST_CHECK(write(fd, contents, sizeof(contents)) == (ssize_t)sizeof(contents));
    TEST_CHECK(close(fd) == 0);

    struct devmgr_worker worker;
    TEST_CHECK(devmgr_worker_init(&worker) == DEVMGR_OK);
    TEST_CHECK(devmgr_worker_submit_firmware(&worker, 7U, path) == DEVMGR_OK);
    TEST_CHECK(devmgr_worker_submit_firmware(&worker, 8U, path) == DEVMGR_ERROR_BUSY);
    struct pollfd descriptor = {.fd = devmgr_worker_get_event_fd(&worker), .events = POLLIN};
    TEST_CHECK(poll(&descriptor, 1U, 3000) == 1);
    uint64_t notifications = 0U;
    TEST_CHECK(read(descriptor.fd, &notifications, sizeof(notifications)) ==
               (ssize_t)sizeof(notifications));
    TEST_CHECK(notifications == 1U);
    struct devmgr_validation_result result;
    TEST_CHECK(devmgr_worker_take_result(&worker, &result) == DEVMGR_OK);
    TEST_CHECK(result.job_id == 7U && result.status == DEVMGR_OK);
    TEST_CHECK(result.image.size == sizeof(contents));
    TEST_CHECK(result.image.crc32 == devmgr_crc32(contents, sizeof(contents)));
    devmgr_firmware_close(&result.image);

    TEST_CHECK(devmgr_worker_submit_firmware(&worker, 8U,
                                              "/tmp/devmgr-worker-does-not-exist") == DEVMGR_OK);
    TEST_CHECK(poll(&descriptor, 1U, 3000) == 1);
    TEST_CHECK(read(descriptor.fd, &notifications, sizeof(notifications)) ==
               (ssize_t)sizeof(notifications));
    TEST_CHECK(devmgr_worker_take_result(&worker, &result) == DEVMGR_OK);
    TEST_CHECK(result.job_id == 8U && result.status == DEVMGR_ERROR_IO);
    TEST_CHECK(result.image.fd == -1 && result.image.data == NULL);
    devmgr_worker_destroy(&worker);
    TEST_CHECK(unlink(path) == 0);
    return 0;
}

int main(void)
{
    int failed = 0;
    TEST_RUN(test_worker_validation);
    return failed == 0 ? 0 : 1;
}
