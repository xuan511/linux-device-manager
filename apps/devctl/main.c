#define _POSIX_C_SOURCE 200809L

#include "devmgr/error.h"
#include "devmgr/ipc.h"
#include "devmgr/protocol.h"
#include "devmgr/upgrade.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
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
    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        (void)close(fd);
        return -1;
    }
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path, length + 1U);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        (void)close(fd);
        return -1;
    }
    return fd;
}

static int exchange(const char *path, uint8_t command, const uint8_t *payload,
                    uint32_t payload_length, struct devmgr_ipc_response *response)
{
    struct devmgr_ipc_request request = {.command = command, .payload_length = payload_length};
    uint8_t buffer[DEVMGR_IPC_RESPONSE_HEADER_SIZE + DEVMGR_IPC_MAX_PAYLOAD];
    size_t length = 0U;
    if (payload_length > DEVMGR_IPC_MAX_PAYLOAD || (payload == NULL && payload_length != 0U))
        return DEVMGR_ERROR_INVALID;
    if (payload_length != 0U) memcpy(request.payload, payload, payload_length);
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
    } else if (command == DEVMGR_MSG_START_TELEMETRY && response->payload_length == 4U) {
        (void)printf("Telemetry started: %u ms\n", devmgr_get_le32(response->payload));
    } else if (command == DEVMGR_MSG_STOP_TELEMETRY) {
        (void)printf("Telemetry stopped\n");
    } else if (command == DEVMGR_IPC_GET_TELEMETRY && response->payload_length == 16U) {
        (void)printf("Temperature: %.3f C\nVoltage: %u mV\nUptime: %u s\nSample: %u\n",
                     (double)(int32_t)devmgr_get_le32(response->payload) / 1000.0,
                     devmgr_get_le32(response->payload + 4U),
                     devmgr_get_le32(response->payload + 8U),
                     devmgr_get_le32(response->payload + 12U));
    } else if (command == DEVMGR_IPC_UPGRADE && response->payload_length == 4U) {
        (void)printf("Upgrade operation: %u\n", devmgr_get_le32(response->payload));
    } else if (command == DEVMGR_IPC_UPGRADE_STATUS && response->payload_length == 20U) {
        enum devmgr_upgrade_state state = (enum devmgr_upgrade_state)response->payload[4];
        uint32_t offset = devmgr_get_le32(response->payload + 12U);
        uint32_t total = devmgr_get_le32(response->payload + 16U);
        (void)printf("Operation: %u\nState: %s\nResult: %s\nProgress: %u/%u\n",
                     devmgr_get_le32(response->payload), devmgr_upgrade_state_string(state),
                     devmgr_status_string((int32_t)devmgr_get_le32(response->payload + 8U)),
                     offset, total);
    }
}

static int wait_for_upgrade(const char *socket_path, const struct devmgr_ipc_response *started)
{
    if (started->status != DEVMGR_OK || started->payload_length != 4U) return 1;
    uint8_t operation_payload[4];
    uint32_t operation_id = devmgr_get_le32(started->payload);
    enum devmgr_upgrade_state previous_state = DEVMGR_UPGRADE_IDLE;
    uint32_t previous_offset = UINT32_MAX;
    devmgr_put_le32(operation_payload, operation_id);
    for (;;) {
        struct devmgr_ipc_response status_response;
        int result = exchange(socket_path, DEVMGR_IPC_UPGRADE_STATUS, operation_payload,
                              sizeof(operation_payload), &status_response);
        if (result != DEVMGR_OK || status_response.status != DEVMGR_OK ||
            status_response.payload_length != 20U ||
            devmgr_get_le32(status_response.payload) != operation_id) {
            (void)fprintf(stderr, "upgrade status IPC failed: %s\n",
                          devmgr_status_string(result != DEVMGR_OK ? result : status_response.status));
            return 1;
        }
        enum devmgr_upgrade_state state =
            (enum devmgr_upgrade_state)status_response.payload[4];
        int operation_result = (int32_t)devmgr_get_le32(status_response.payload + 8U);
        uint32_t offset = devmgr_get_le32(status_response.payload + 12U);
        uint32_t total = devmgr_get_le32(status_response.payload + 16U);
        if (state != previous_state || offset != previous_offset) {
            unsigned percent = total == 0U ? 0U : (unsigned)((uint64_t)offset * 100U / total);
            (void)printf("Upgrade %-16s %3u%% (%u/%u)\n",
                         devmgr_upgrade_state_string(state), percent, offset, total);
            previous_state = state;
            previous_offset = offset;
        }
        if (state == DEVMGR_UPGRADE_COMPLETE) {
            (void)printf("Firmware upgrade: verified, activated, rebooted\n");
            return 0;
        }
        if (state == DEVMGR_UPGRADE_ERROR) {
            (void)fprintf(stderr, "firmware upgrade failed: %s\n",
                          devmgr_status_string(operation_result));
            return 1;
        }
        struct timespec interval = {.tv_sec = 0, .tv_nsec = 100000000L};
        while (nanosleep(&interval, &interval) < 0 && errno == EINTR) {}
    }
}

static void usage(const char *program)
{
    (void)fprintf(stderr, "Usage: %s [--socket PATH] COMMAND\n"
                          "Commands: ping, info, health, stats, telemetry-start [ms], "
                          "telemetry-stop, telemetry, upgrade FILE [VERSION], "
                          "upgrade-status ID\n", program);
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
    if (optind >= argc || optind + 3 < argc) { usage(argv[0]); return 2; }
    uint8_t command;
    uint8_t request_payload[DEVMGR_IPC_MAX_PAYLOAD];
    uint32_t request_payload_length = 0U;
    if (strcmp(argv[optind], "ping") == 0) command = DEVMGR_MSG_PING;
    else if (strcmp(argv[optind], "info") == 0) command = DEVMGR_MSG_GET_INFO;
    else if (strcmp(argv[optind], "health") == 0) command = DEVMGR_MSG_GET_HEALTH;
    else if (strcmp(argv[optind], "stats") == 0) command = DEVMGR_MSG_GET_STATS;
    else if (strcmp(argv[optind], "telemetry-start") == 0) {
        if (optind + 2 < argc) { usage(argv[0]); return 2; }
        char *end = NULL;
        unsigned long interval = optind + 1 < argc ? strtoul(argv[optind + 1], &end, 10) : 1000UL;
        if ((end != NULL && *end != '\0') || interval < 100UL || interval > 60000UL) {
            (void)fprintf(stderr, "interval must be 100..60000 ms\n"); return 2;
        }
        command = DEVMGR_MSG_START_TELEMETRY;
        devmgr_put_le32(request_payload, (uint32_t)interval);
        request_payload_length = 4U;
    }
    else if (strcmp(argv[optind], "telemetry-stop") == 0) command = DEVMGR_MSG_STOP_TELEMETRY;
    else if (strcmp(argv[optind], "telemetry") == 0) command = DEVMGR_IPC_GET_TELEMETRY;
    else if (strcmp(argv[optind], "upgrade") == 0) {
        char resolved[PATH_MAX];
        if (optind + 1 >= argc || optind + 3 < argc ||
            realpath(argv[optind + 1], resolved) == NULL) {
            (void)fprintf(stderr, "cannot resolve firmware file\n"); return 2;
        }
        const char *version = optind + 2 < argc ? argv[optind + 2] : "1.1.0";
        size_t path_length = strlen(resolved) + 1U;
        size_t version_length = strlen(version) + 1U;
        if (path_length + version_length > sizeof(request_payload)) {
            (void)fprintf(stderr, "path/version too long\n"); return 2;
        }
        memcpy(request_payload, resolved, path_length);
        memcpy(request_payload + path_length, version, version_length);
        request_payload_length = (uint32_t)(path_length + version_length);
        command = DEVMGR_IPC_UPGRADE;
    }
    else if (strcmp(argv[optind], "upgrade-status") == 0) {
        if (optind + 2 != argc) { usage(argv[0]); return 2; }
        char *end = NULL;
        unsigned long operation = optind + 1 < argc ? strtoul(argv[optind + 1], &end, 10) : 0UL;
        if (operation == 0UL || operation > UINT32_MAX || end == NULL || *end != '\0') {
            (void)fprintf(stderr, "invalid operation ID\n"); return 2;
        }
        devmgr_put_le32(request_payload, (uint32_t)operation);
        request_payload_length = 4U;
        command = DEVMGR_IPC_UPGRADE_STATUS;
    }
    else { usage(argv[0]); return 2; }
    if (command != DEVMGR_MSG_START_TELEMETRY && command != DEVMGR_IPC_UPGRADE &&
        command != DEVMGR_IPC_UPGRADE_STATUS &&
        optind + 1 < argc) { usage(argv[0]); return 2; }
    struct timespec before, after;
    struct devmgr_ipc_response response;
    (void)clock_gettime(CLOCK_MONOTONIC, &before);
    int result = exchange(socket_path, command, request_payload, request_payload_length, &response);
    (void)clock_gettime(CLOCK_MONOTONIC, &after);
    if (result != DEVMGR_OK) {
        (void)fprintf(stderr, "IPC failed: %s\n", devmgr_status_string(result)); return 1;
    }
    uint64_t elapsed = (uint64_t)(after.tv_sec - before.tv_sec) * UINT64_C(1000000000);
    if (after.tv_nsec >= before.tv_nsec) elapsed += (uint64_t)(after.tv_nsec - before.tv_nsec);
    else elapsed -= (uint64_t)(before.tv_nsec - after.tv_nsec);
    print_response(command, &response, elapsed);
    if (command == DEVMGR_IPC_UPGRADE && response.status == DEVMGR_OK)
        return wait_for_upgrade(socket_path, &response);
    return response.status == DEVMGR_OK ? 0 : 1;
}
