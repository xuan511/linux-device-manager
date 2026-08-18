#define _POSIX_C_SOURCE 200809L

#include "devmgr/error.h"
#include "devmgr/ipc.h"
#include "devmgr/protocol.h"

#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int write_all(int fd, const uint8_t *data, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        ssize_t count = write(fd, data + offset, length - offset);
        if (count > 0) offset += (size_t)count;
        else if (count < 0 && errno == EINTR) continue;
        else return DEVMGR_ERROR_IO;
    }
    return DEVMGR_OK;
}

static int read_all(int fd, uint8_t *data, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        ssize_t count = read(fd, data + offset, length - offset);
        if (count > 0) offset += (size_t)count;
        else if (count < 0 && errno == EINTR) continue;
        else return count == 0 ? DEVMGR_ERROR_DISCONNECTED : DEVMGR_ERROR_IO;
    }
    return DEVMGR_OK;
}

static int connect_socket(const char *path)
{
    struct sockaddr_un address;
    size_t length = strlen(path);
    if (length == 0U || length >= sizeof(address.sun_path)) return -1;
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path, length + 1U);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        (void)close(fd);
        return -1;
    }
    return fd;
}

static int exchange(const char *path, uint8_t command, struct devmgr_ipc_response *response)
{
    struct devmgr_ipc_request request = {.command = command};
    uint8_t buffer[DEVMGR_IPC_RESPONSE_HEADER_SIZE + DEVMGR_IPC_MAX_PAYLOAD];
    size_t length = 0U;
    int fd = connect_socket(path);
    if (fd < 0) return DEVMGR_ERROR_IO;
    int result = devmgr_ipc_encode_request(&request, buffer, sizeof(buffer), &length);
    if (result == DEVMGR_OK) result = write_all(fd, buffer, length);
    if (result == DEVMGR_OK) result = read_all(fd, buffer, DEVMGR_IPC_RESPONSE_HEADER_SIZE);
    if (result == DEVMGR_OK) {
        uint32_t payload_length = devmgr_get_le32(buffer + 8U);
        if (payload_length > DEVMGR_IPC_MAX_PAYLOAD) result = DEVMGR_ERROR_LIMIT;
        else {
            result = read_all(fd, buffer + DEVMGR_IPC_RESPONSE_HEADER_SIZE, payload_length);
            length = DEVMGR_IPC_RESPONSE_HEADER_SIZE + payload_length;
        }
    }
    if (result == DEVMGR_OK) result = devmgr_ipc_decode_response(buffer, length, response);
    (void)close(fd);
    return result;
}

static const char *next_string(const uint8_t *payload, size_t length, size_t *offset)
{
    if (*offset >= length) return NULL;
    const uint8_t *end = memchr(payload + *offset, 0, length - *offset);
    if (end == NULL) return NULL;
    const char *value = (const char *)(payload + *offset);
    *offset = (size_t)(end - payload) + 1U;
    return value;
}

static void print_response(uint8_t command, const struct devmgr_ipc_response *response,
                           uint64_t elapsed_ns)
{
    if (response->status != DEVMGR_OK) {
        (void)fprintf(stderr, "command failed: %s\n", devmgr_status_string(response->status));
        return;
    }
    if (command == DEVMGR_MSG_PING) {
        (void)printf("RTT: %.3f ms\n", (double)elapsed_ns / 1000000.0);
    } else if (command == DEVMGR_MSG_GET_INFO && response->payload_length >= 4U) {
        size_t offset = 4U;
        const char *model = next_string(response->payload, response->payload_length, &offset);
        const char *hardware = next_string(response->payload, response->payload_length, &offset);
        const char *firmware = next_string(response->payload, response->payload_length, &offset);
        const char *bootloader = next_string(response->payload, response->payload_length, &offset);
        const char *serial = next_string(response->payload, response->payload_length, &offset);
        if (model == NULL || hardware == NULL || firmware == NULL || bootloader == NULL || serial == NULL) {
            (void)fprintf(stderr, "malformed device info\n"); return;
        }
        (void)printf("Device ID: %08x\nModel: %s\nHardware: %s\nFirmware: %s\nBootloader: %s\nSerial: %s\n",
                     devmgr_get_le32(response->payload), model, hardware, firmware, bootloader, serial);
    } else if (command == DEVMGR_MSG_GET_HEALTH && response->payload_length == 12U) {
        (void)printf("Temperature: %.3f C\nVoltage: %u mV\nFlags: 0x%08x\n",
                     (double)(int32_t)devmgr_get_le32(response->payload) / 1000.0,
                     devmgr_get_le32(response->payload + 4U),
                     devmgr_get_le32(response->payload + 8U));
    } else if (command == DEVMGR_MSG_GET_STATS && response->payload_length == 12U) {
        (void)printf("RX frames: %u\nTX frames: %u\nErrors: %u\n",
                     devmgr_get_le32(response->payload), devmgr_get_le32(response->payload + 4U),
                     devmgr_get_le32(response->payload + 8U));
    }
}

static void usage(const char *program)
{
    (void)fprintf(stderr, "Usage: %s [--socket PATH] ping|info|health|stats\n", program);
}

int main(int argc, char **argv)
{
    static const struct option options[] = {{"socket", required_argument, NULL, 's'},
                                             {"help", no_argument, NULL, 'h'},
                                             {NULL, 0, NULL, 0}};
    char default_socket[128];
    const char *socket_path = default_socket;
    int option;
    (void)snprintf(default_socket, sizeof(default_socket), "/tmp/devmgrd-%lu.sock",
                   (unsigned long)getuid());
    while ((option = getopt_long(argc, argv, "s:h", options, NULL)) != -1) {
        if (option == 's') socket_path = optarg;
        else { usage(argv[0]); return option == 'h' ? 0 : 2; }
    }
    if (optind + 1 != argc) { usage(argv[0]); return 2; }
    uint8_t command;
    if (strcmp(argv[optind], "ping") == 0) command = DEVMGR_MSG_PING;
    else if (strcmp(argv[optind], "info") == 0) command = DEVMGR_MSG_GET_INFO;
    else if (strcmp(argv[optind], "health") == 0) command = DEVMGR_MSG_GET_HEALTH;
    else if (strcmp(argv[optind], "stats") == 0) command = DEVMGR_MSG_GET_STATS;
    else { usage(argv[0]); return 2; }
    struct timespec before, after;
    struct devmgr_ipc_response response;
    (void)clock_gettime(CLOCK_MONOTONIC, &before);
    int result = exchange(socket_path, command, &response);
    (void)clock_gettime(CLOCK_MONOTONIC, &after);
    if (result != DEVMGR_OK) {
        (void)fprintf(stderr, "IPC failed: %s\n", devmgr_status_string(result)); return 1;
    }
    uint64_t elapsed = (uint64_t)(after.tv_sec - before.tv_sec) * UINT64_C(1000000000);
    if (after.tv_nsec >= before.tv_nsec) elapsed += (uint64_t)(after.tv_nsec - before.tv_nsec);
    else elapsed -= (uint64_t)(before.tv_nsec - after.tv_nsec);
    print_response(command, &response, elapsed);
    return response.status == DEVMGR_OK ? 0 : 1;
}

