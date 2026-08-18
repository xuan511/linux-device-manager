#include "simulator.h"

#include "devmgr/error.h"
#include "devmgr/log.h"
#include "devmgr/protocol.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested;

static uint64_t simulator_now_ns(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        return 0U;
    }
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

void simulator_request_stop(void)
{
    stop_requested = 1;
}

static int write_frame(struct simulator_state *simulator, const struct devmgr_frame *frame)
{
    uint8_t encoded[DEVMGR_PROTOCOL_MAX_FRAME];
    size_t length = 0U;
    size_t offset = 0U;
    int result = devmgr_frame_encode(frame, encoded, sizeof(encoded), &length);

    if (result != DEVMGR_OK) {
        return result;
    }
    while (offset < length) {
        ssize_t count = write(simulator->master_fd, encoded + offset, length - offset);
        if (count > 0) {
            offset += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd descriptor = {.fd = simulator->master_fd, .events = POLLOUT};
            if (poll(&descriptor, 1, 1000) > 0) {
                continue;
            }
        }
        return DEVMGR_ERROR_IO;
    }
    ++simulator->tx_frames;
    return DEVMGR_OK;
}

static void put_string(uint8_t *payload, size_t capacity, size_t *offset, const char *value)
{
    size_t length = strlen(value) + 1U;
    if (*offset <= capacity && length <= capacity - *offset) {
        memcpy(payload + *offset, value, length);
        *offset += length;
    }
}

static int on_frame(const struct devmgr_frame *request, void *context)
{
    struct simulator_state *simulator = context;
    struct devmgr_frame response = {0};
    size_t offset = 0U;

    ++simulator->rx_frames;
    response.type = request->type;
    response.flags = DEVMGR_FRAME_RESPONSE;
    response.sequence = request->sequence;
    switch (request->type) {
    case DEVMGR_MSG_PING:
        response.payload_length = request->payload_length;
        memcpy(response.payload, request->payload, request->payload_length);
        break;
    case DEVMGR_MSG_GET_INFO:
        devmgr_put_le32(response.payload, simulator->device_id);
        offset = 4U;
        put_string(response.payload, sizeof(response.payload), &offset, "STM32-SIM");
        put_string(response.payload, sizeof(response.payload), &offset, "HW-1.0");
        put_string(response.payload, sizeof(response.payload), &offset, "1.0.0");
        put_string(response.payload, sizeof(response.payload), &offset, "BL-1.0.0");
        put_string(response.payload, sizeof(response.payload), &offset, "SIM00000001");
        response.payload_length = (uint16_t)offset;
        break;
    case DEVMGR_MSG_GET_HEALTH:
        devmgr_put_le32(response.payload, (uint32_t)simulator->temperature_millic);
        devmgr_put_le32(response.payload + 4U, simulator->voltage_mv);
        devmgr_put_le32(response.payload + 8U, 0U);
        response.payload_length = 12U;
        break;
    case DEVMGR_MSG_GET_STATS:
        devmgr_put_le32(response.payload, (uint32_t)simulator->rx_frames);
        devmgr_put_le32(response.payload + 4U, (uint32_t)simulator->tx_frames);
        devmgr_put_le32(response.payload + 8U, (uint32_t)simulator->errors);
        response.payload_length = 12U;
        break;
    case DEVMGR_MSG_START_TELEMETRY:
        if (request->payload_length != 4U) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 3U);
            response.payload_length = 2U;
            break;
        }
        simulator->telemetry_interval_ms = devmgr_get_le32(request->payload);
        if (simulator->telemetry_interval_ms < 100U || simulator->telemetry_interval_ms > 60000U) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 3U);
            response.payload_length = 2U;
            break;
        }
        simulator->telemetry_enabled = true;
        simulator->next_telemetry_ns = simulator_now_ns() +
                                       (uint64_t)simulator->telemetry_interval_ms * UINT64_C(1000000);
        devmgr_put_le32(response.payload, simulator->telemetry_interval_ms);
        response.payload_length = 4U;
        break;
    case DEVMGR_MSG_STOP_TELEMETRY:
        simulator->telemetry_enabled = false;
        response.payload_length = 0U;
        break;
    default:
        response.type = DEVMGR_MSG_NACK;
        devmgr_put_le16(response.payload, 1U);
        response.payload_length = 2U;
        break;
    }
    return write_frame(simulator, &response);
}

static int emit_telemetry(struct simulator_state *simulator, uint64_t now_ns)
{
    struct devmgr_frame frame = {.type = DEVMGR_MSG_TELEMETRY,
                                 .flags = DEVMGR_FRAME_RESPONSE,
                                 .sequence = 0U,
                                 .payload_length = 16U};
    uint64_t started_ns = (uint64_t)simulator->started_at.tv_sec * UINT64_C(1000000000) +
                          (uint64_t)simulator->started_at.tv_nsec;
    uint32_t uptime_seconds = (uint32_t)((now_ns - started_ns) / UINT64_C(1000000000));
    int32_t wave = (int32_t)(simulator->telemetry_samples % 21U) - 10;
    simulator->temperature_millic = 36500 + wave * 25;
    simulator->voltage_mv = 3300U - (simulator->telemetry_samples % 7U);
    devmgr_put_le32(frame.payload, (uint32_t)simulator->temperature_millic);
    devmgr_put_le32(frame.payload + 4U, simulator->voltage_mv);
    devmgr_put_le32(frame.payload + 8U, uptime_seconds);
    devmgr_put_le32(frame.payload + 12U, simulator->telemetry_samples++);
    simulator->next_telemetry_ns = now_ns +
                                   (uint64_t)simulator->telemetry_interval_ms * UINT64_C(1000000);
    return write_frame(simulator, &frame);
}

int simulator_init(struct simulator_state *simulator, int master_fd)
{
    if (simulator == NULL || master_fd < 0) {
        return DEVMGR_ERROR_INVALID;
    }
    memset(simulator, 0, sizeof(*simulator));
    simulator->master_fd = master_fd;
    simulator->device_id = UINT32_C(0x53494D30);
    simulator->temperature_millic = 36500;
    simulator->voltage_mv = 3300U;
    simulator->running = true;
    (void)clock_gettime(CLOCK_MONOTONIC, &simulator->started_at);
    return devmgr_parser_init(&simulator->parser);
}

int simulator_run(struct simulator_state *simulator)
{
    uint8_t input[2048];

    if (simulator == NULL) {
        return DEVMGR_ERROR_INVALID;
    }
    while (simulator->running && !stop_requested) {
        struct pollfd descriptor = {.fd = simulator->master_fd, .events = POLLIN};
        int ready = poll(&descriptor, 1, simulator->telemetry_enabled ? 100 : 1000);
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready < 0) {
            return DEVMGR_ERROR_IO;
        }
        uint64_t now_ns = simulator_now_ns();
        if (simulator->telemetry_enabled && now_ns >= simulator->next_telemetry_ns &&
            emit_telemetry(simulator, now_ns) != DEVMGR_OK) return DEVMGR_ERROR_IO;
        if (ready == 0) continue;
        if ((descriptor.revents & (POLLERR | POLLNVAL)) != 0) {
            return DEVMGR_ERROR_IO;
        }
        if ((descriptor.revents & POLLIN) != 0) {
            ssize_t count = read(simulator->master_fd, input, sizeof(input));
            if (count > 0) {
                size_t emitted = 0U;
                int result = devmgr_parser_feed(&simulator->parser, input, (size_t)count, on_frame,
                                                simulator, &emitted);
                if (result != DEVMGR_OK) {
                    ++simulator->errors;
                    devmgr_log_write(DEVMGR_LOG_WARN, "sim", "parser: %s",
                                     devmgr_status_string(result));
                }
            } else if (count < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK &&
                       errno != EIO) {
                return DEVMGR_ERROR_IO;
            }
        }
    }
    return DEVMGR_OK;
}
