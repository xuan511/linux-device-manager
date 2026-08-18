#include "test.h"

#include "devmgr/error.h"
#include "devmgr/pty.h"
#include "devmgr/transport.h"

#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static int wait_readable(int fd)
{
    struct pollfd descriptor = {.fd = fd, .events = POLLIN};
    return poll(&descriptor, 1, 1000) > 0 ? 0 : 1;
}

static int test_pty_transport(void)
{
    char path[256];
    int master_fd = -1;
    struct devmgr_transport transport;
    struct devmgr_transport_config config = {
        .kind = DEVMGR_TRANSPORT_PTY, .path = path, .baud_rate = 115200U};
    static const uint8_t outbound[] = {1U, 2U, 3U, 4U};
    static const uint8_t inbound[] = {5U, 6U, 7U};
    uint8_t buffer[8] = {0U};

    TEST_CHECK(devmgr_pty_create(&master_fd, path, sizeof(path)) == DEVMGR_OK);
    TEST_CHECK(devmgr_transport_open(&transport, &config) == DEVMGR_OK);
    TEST_CHECK(devmgr_transport_write(&transport, outbound, sizeof(outbound)) ==
               (ssize_t)sizeof(outbound));
    TEST_CHECK(wait_readable(master_fd) == 0);
    TEST_CHECK(read(master_fd, buffer, sizeof(buffer)) == (ssize_t)sizeof(outbound));
    TEST_CHECK(memcmp(buffer, outbound, sizeof(outbound)) == 0);
    TEST_CHECK(write(master_fd, inbound, sizeof(inbound)) == (ssize_t)sizeof(inbound));
    TEST_CHECK(wait_readable(devmgr_transport_get_fd(&transport)) == 0);
    TEST_CHECK(devmgr_transport_read(&transport, buffer, sizeof(buffer)) == (ssize_t)sizeof(inbound));
    TEST_CHECK(memcmp(buffer, inbound, sizeof(inbound)) == 0);
    devmgr_transport_close(&transport);
    TEST_CHECK(devmgr_transport_get_fd(&transport) == -1);
    TEST_CHECK(close(master_fd) == 0);
    return 0;
}

int main(void)
{
    int failed = 0;
    TEST_RUN(test_pty_transport);
    return failed == 0 ? 0 : 1;
}

