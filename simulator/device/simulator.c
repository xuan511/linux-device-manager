#include "simulator.h"

#include "devmgr/error.h"
#include "devmgr/crc32.h"
#include "devmgr/firmware.h"
#include "devmgr/log.h"
#include "devmgr/protocol.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested;

static double simulator_random(struct simulator_state *simulator)
{
    uint32_t value = simulator->random_state;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    simulator->random_state = value;
    return (double)value / 4294967296.0;
}

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
    uint64_t now_ns = simulator_now_ns();
    if (now_ns < simulator->outage_until_ns || simulator_random(simulator) < simulator->drop_rate)
        return DEVMGR_OK;
    int result = devmgr_frame_encode(frame, encoded, sizeof(encoded), &length);

    if (result != DEVMGR_OK) {
        return result;
    }
    if (simulator_random(simulator) < simulator->corrupt_rate && length > DEVMGR_PROTOCOL_HEADER_SIZE)
        encoded[DEVMGR_PROTOCOL_HEADER_SIZE] ^= 0x40U;
    if (simulator->response_delay_ms != 0U) {
        struct timespec delay = {.tv_sec = (time_t)(simulator->response_delay_ms / 1000U),
                                 .tv_nsec = (long)(simulator->response_delay_ms % 1000U) * 1000000L};
        while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
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
        put_string(response.payload, sizeof(response.payload), &offset, simulator->firmware_version);
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
    case DEVMGR_MSG_ENTER_BOOTLOADER:
        simulator->telemetry_enabled = false;
        simulator->bootloader_active = true;
        response.payload_length = 0U;
        break;
    case DEVMGR_MSG_FW_BEGIN: {
        if (!simulator->bootloader_active || request->payload_length < 11U) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 2U);
            response.payload_length = 2U;
            break;
        }
        uint32_t image_size = devmgr_get_le32(request->payload);
        uint32_t image_crc = devmgr_get_le32(request->payload + 4U);
        uint16_t chunk_size = devmgr_get_le16(request->payload + 8U);
        const uint8_t *version_end = memchr(request->payload + 10U, 0,
                                             request->payload_length - 10U);
        size_t version_length = version_end == NULL ? 0U :
                                (size_t)(version_end - (request->payload + 10U));
        if (image_size == 0U || image_size > DEVMGR_FIRMWARE_MAX_SIZE || chunk_size == 0U ||
            chunk_size > DEVMGR_PROTOCOL_MAX_PAYLOAD - 8U || version_length == 0U ||
            version_length >= sizeof(simulator->pending_version)) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 3U);
            response.payload_length = 2U;
            break;
        }
        uint8_t *new_flash = malloc(image_size);
        if (new_flash == NULL) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 6U);
            response.payload_length = 2U;
            break;
        }
        free(simulator->flash);
        simulator->flash = new_flash;
        simulator->flash_size = image_size;
        simulator->expected_firmware_crc = image_crc;
        simulator->firmware_chunk_size = chunk_size;
        simulator->firmware_offset = 0U;
        simulator->firmware_session_id++;
        if (simulator->firmware_session_id == 0U) simulator->firmware_session_id = 1U;
        memcpy(simulator->pending_version, request->payload + 10U, version_length + 1U);
        simulator->firmware_upgrading = true;
        simulator->firmware_verified = false;
        devmgr_put_le32(response.payload, simulator->firmware_session_id);
        devmgr_put_le32(response.payload + 4U, simulator->firmware_offset);
        response.payload_length = 8U;
        break;
    }
    case DEVMGR_MSG_FW_DATA: {
        if (!simulator->firmware_upgrading || request->payload_length <= 8U ||
            devmgr_get_le32(request->payload) != simulator->firmware_session_id) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 2U);
            response.payload_length = 2U;
            break;
        }
        uint32_t offset_value = devmgr_get_le32(request->payload + 4U);
        size_t data_length = request->payload_length - 8U;
        if (data_length > simulator->firmware_chunk_size || offset_value > simulator->flash_size ||
            data_length > simulator->flash_size - offset_value) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 5U);
            response.payload_length = 2U;
            break;
        }
        if (offset_value < simulator->firmware_offset) {
            if (offset_value + data_length > simulator->firmware_offset ||
                memcmp(simulator->flash + offset_value, request->payload + 8U, data_length) != 0) {
                response.type = DEVMGR_MSG_NACK;
                devmgr_put_le16(response.payload, 5U);
                response.payload_length = 2U;
                break;
            }
        } else if (offset_value == simulator->firmware_offset) {
            memcpy(simulator->flash + offset_value, request->payload + 8U, data_length);
            simulator->firmware_offset += (uint32_t)data_length;
        } else {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 5U);
            response.payload_length = 2U;
            break;
        }
        devmgr_put_le32(response.payload, simulator->firmware_offset);
        response.payload_length = 4U;
        if (!simulator->outage_injected && simulator->disconnect_at_percent >= 0 &&
            simulator->flash_size != 0U &&
            (uint64_t)simulator->firmware_offset * 100U / simulator->flash_size >=
                (uint64_t)simulator->disconnect_at_percent) {
            simulator->outage_injected = true;
            simulator->outage_until_ns = simulator_now_ns() + UINT64_C(6000000000);
        }
        break;
    }
    case DEVMGR_MSG_FW_STATUS:
        if (!simulator->firmware_upgrading || request->payload_length != 4U ||
            devmgr_get_le32(request->payload) != simulator->firmware_session_id) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 2U);
            response.payload_length = 2U;
        } else {
            devmgr_put_le32(response.payload, simulator->firmware_session_id);
            devmgr_put_le32(response.payload + 4U, simulator->firmware_offset);
            response.payload_length = 8U;
        }
        break;
    case DEVMGR_MSG_FW_END:
        if (!simulator->firmware_upgrading || request->payload_length != 4U ||
            simulator->fail_verify ||
            devmgr_get_le32(request->payload) != simulator->firmware_session_id ||
            simulator->firmware_offset != simulator->flash_size) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 5U);
            response.payload_length = 2U;
        }
        break;
    case DEVMGR_MSG_FW_VERIFY: {
        uint32_t actual_crc = simulator->flash == NULL ? 0U :
                              devmgr_crc32(simulator->flash, simulator->flash_size);
        if (!simulator->firmware_upgrading || request->payload_length != 4U ||
            actual_crc != simulator->expected_firmware_crc) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 7U);
            response.payload_length = 2U;
        } else {
            simulator->firmware_verified = true;
            devmgr_put_le32(response.payload, actual_crc);
            response.payload_length = 4U;
        }
        break;
    }
    case DEVMGR_MSG_FW_ACTIVATE:
        if (!simulator->firmware_verified) {
            response.type = DEVMGR_MSG_NACK;
            devmgr_put_le16(response.payload, 2U);
            response.payload_length = 2U;
        } else {
            memcpy(simulator->firmware_version, simulator->pending_version,
                   strlen(simulator->pending_version) + 1U);
        }
        break;
    case DEVMGR_MSG_REBOOT:
        simulator->bootloader_active = false;
        simulator->firmware_upgrading = false;
        simulator->firmware_verified = false;
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

int simulator_init(struct simulator_state *simulator, int master_fd,
                   const struct simulator_config *config)
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
    simulator->disconnect_at_percent = -1;
    simulator->random_state = UINT32_C(0xC0FFEE01);
    if (config != NULL) {
        simulator->drop_rate = config->drop_rate;
        simulator->corrupt_rate = config->corrupt_rate;
        simulator->response_delay_ms = config->response_delay_ms;
        simulator->disconnect_at_percent = config->disconnect_at_percent;
        simulator->fail_verify = config->fail_verify;
        if (config->random_seed != 0U) simulator->random_state = config->random_seed;
    }
    memcpy(simulator->firmware_version, "1.0.0", sizeof("1.0.0"));
    (void)clock_gettime(CLOCK_MONOTONIC, &simulator->started_at);
    return devmgr_parser_init(&simulator->parser);
}

void simulator_cleanup(struct simulator_state *simulator)
{
    if (simulator != NULL) {
        free(simulator->flash);
        simulator->flash = NULL;
        simulator->flash_size = 0U;
    }
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
