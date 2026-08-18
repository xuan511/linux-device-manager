#define _GNU_SOURCE

#include "daemon.h"

#include "devmgr/error.h"
#include "devmgr/firmware.h"
#include "devmgr/ipc.h"
#include "devmgr/log.h"
#include "devmgr/parser.h"
#include "devmgr/protocol.h"
#include "devmgr/session.h"
#include "devmgr/transport.h"
#include "devmgr/upgrade.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 16U

struct daemon_client {
    int fd;
    uint8_t input[DEVMGR_IPC_REQUEST_HEADER_SIZE + DEVMGR_IPC_MAX_PAYLOAD];
    size_t input_size;
    size_t expected_size;
    uint8_t output[DEVMGR_IPC_RESPONSE_HEADER_SIZE + DEVMGR_IPC_MAX_PAYLOAD];
    size_t output_size;
    size_t output_offset;
    bool close_after_write;
};

struct daemon_context {
    int epoll_fd;
    int listen_fd;
    int timer_fd;
    int signal_fd;
    struct devmgr_transport transport;
    struct devmgr_parser parser;
    struct devmgr_session session;
    struct daemon_client clients[MAX_CLIENTS];
    int active_client;
    uint8_t transport_output[DEVMGR_PROTOCOL_MAX_FRAME];
    size_t transport_output_size;
    size_t transport_output_offset;
    const char *socket_path;
    bool running;
    uint8_t latest_telemetry[16];
    size_t latest_telemetry_length;
    uint64_t telemetry_frames;
    struct devmgr_firmware_image firmware;
    struct devmgr_upgrade upgrade;
};

static int queue_upgrade_request(struct daemon_context *context);

static uint64_t monotonic_ns(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        return 0U;
    }
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

static int epoll_update(struct daemon_context *context, int operation, int fd, uint32_t events)
{
    struct epoll_event event = {.events = events, .data.fd = fd};
    if (epoll_ctl(context->epoll_fd, operation, fd, &event) < 0) {
        return DEVMGR_ERROR_IO;
    }
    return DEVMGR_OK;
}

static void close_client(struct daemon_context *context, size_t index)
{
    struct daemon_client *client = &context->clients[index];
    if (client->fd >= 0) {
        (void)epoll_ctl(context->epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
        (void)close(client->fd);
        client->fd = -1;
    }
    if (context->active_client == (int)index) {
        context->active_client = -1;
    }
}

static int queue_client_response(struct daemon_context *context, size_t index, uint8_t command,
                                 int status, const uint8_t *payload, size_t payload_length)
{
    struct daemon_client *client = &context->clients[index];
    struct devmgr_ipc_response response = {.command = command, .status = status};
    int result;

    if (payload_length > DEVMGR_IPC_MAX_PAYLOAD) {
        payload_length = 0U;
        response.status = DEVMGR_ERROR_LIMIT;
    }
    response.payload_length = (uint32_t)payload_length;
    if (payload_length != 0U && payload != NULL) {
        memcpy(response.payload, payload, payload_length);
    }
    result = devmgr_ipc_encode_response(&response, client->output, sizeof(client->output),
                                        &client->output_size);
    if (result != DEVMGR_OK) {
        return result;
    }
    client->output_offset = 0U;
    client->close_after_write = true;
    return epoll_update(context, EPOLL_CTL_MOD, client->fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
}

static int update_transport_events(struct daemon_context *context)
{
    uint32_t events = EPOLLIN | EPOLLRDHUP;
    if (context->transport_output_offset < context->transport_output_size) {
        events |= EPOLLOUT;
    }
    return epoll_update(context, EPOLL_CTL_MOD, devmgr_transport_get_fd(&context->transport), events);
}

static int queue_frame(struct daemon_context *context, const struct devmgr_frame *frame)
{
    if (context->transport_output_offset < context->transport_output_size) {
        return DEVMGR_ERROR_BUSY;
    }
    context->transport_output_offset = 0U;
    int result = devmgr_frame_encode(frame, context->transport_output,
                                     sizeof(context->transport_output),
                                     &context->transport_output_size);
    if (result != DEVMGR_OK) {
        return result;
    }
    return update_transport_events(context);
}

static int retransmit_frame(const struct devmgr_frame *frame, void *opaque)
{
    return queue_frame(opaque, frame);
}

static int on_device_frame(const struct devmgr_frame *frame, void *opaque)
{
    struct daemon_context *context = opaque;
    if (frame->type == DEVMGR_MSG_TELEMETRY && frame->sequence == 0U) {
        if (frame->payload_length == sizeof(context->latest_telemetry)) {
            memcpy(context->latest_telemetry, frame->payload, frame->payload_length);
            context->latest_telemetry_length = frame->payload_length;
            ++context->telemetry_frames;
        }
        return DEVMGR_OK;
    }
    uint8_t request_type = context->session.pending.frame.type;
    int result = devmgr_session_accept_response(&context->session, frame);
    if (result != DEVMGR_OK) {
        devmgr_log_write(DEVMGR_LOG_WARN, "daemon", "ignored sequence %u: %s", frame->sequence,
                         devmgr_status_string(result));
        return DEVMGR_OK;
    }
    if (context->session.state == DEVMGR_DEVICE_HANDSHAKING) {
        result = devmgr_session_transition(&context->session, DEVMGR_SESSION_HANDSHAKE_OK);
        if (result == DEVMGR_OK) {
            devmgr_log_write(DEVMGR_LOG_INFO, "daemon", "device is READY");
        }
        return result;
    }
    if (devmgr_upgrade_active(&context->upgrade)) {
        result = devmgr_upgrade_accept_response(&context->upgrade, request_type, frame);
        if (result == DEVMGR_OK && context->upgrade.state == DEVMGR_UPGRADE_COMPLETE) {
            if (context->active_client >= 0) {
                size_t index = (size_t)context->active_client;
                context->active_client = -1;
                result = queue_client_response(context, index, DEVMGR_IPC_UPGRADE, DEVMGR_OK,
                                               NULL, 0U);
            }
            (void)devmgr_session_transition(&context->session, DEVMGR_SESSION_UPGRADE_DONE);
            devmgr_firmware_close(&context->firmware);
            return result;
        }
        if (result != DEVMGR_OK) {
            if (context->active_client >= 0) {
                size_t index = (size_t)context->active_client;
                context->active_client = -1;
                (void)queue_client_response(context, index, DEVMGR_IPC_UPGRADE, result, NULL, 0U);
            }
            (void)devmgr_session_transition(&context->session, DEVMGR_SESSION_UPGRADE_DONE);
            devmgr_firmware_close(&context->firmware);
            return DEVMGR_OK;
        }
        return queue_upgrade_request(context);
    }
    if (context->active_client >= 0) {
        size_t index = (size_t)context->active_client;
        context->active_client = -1;
        if (frame->type != DEVMGR_MSG_NACK && frame->type != DEVMGR_MSG_ERROR) {
            if (request_type == DEVMGR_MSG_START_TELEMETRY)
                (void)devmgr_session_transition(&context->session, DEVMGR_SESSION_START_STREAM);
            else if (request_type == DEVMGR_MSG_STOP_TELEMETRY)
                (void)devmgr_session_transition(&context->session, DEVMGR_SESSION_STOP_STREAM);
        }
        return queue_client_response(context, index, request_type, DEVMGR_OK, frame->payload,
                                     frame->payload_length);
    }
    return DEVMGR_OK;
}

static int flush_transport(struct daemon_context *context)
{
    while (context->transport_output_offset < context->transport_output_size) {
        ssize_t count = devmgr_transport_write(
            &context->transport, context->transport_output + context->transport_output_offset,
            context->transport_output_size - context->transport_output_offset);
        if (count > 0) {
            context->transport_output_offset += (size_t)count;
        } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        } else {
            return DEVMGR_ERROR_DISCONNECTED;
        }
    }
    if (context->transport_output_offset == context->transport_output_size) {
        context->transport_output_offset = 0U;
        context->transport_output_size = 0U;
    }
    return update_transport_events(context);
}

static int queue_upgrade_request(struct daemon_context *context)
{
    struct devmgr_frame upgrade_frame;
    struct devmgr_frame request;
    struct devmgr_retry_policy policy = {.timeout_ms = 2000U, .retry_interval_ms = 750U,
                                         .max_retries = 4U};
    int result = devmgr_upgrade_build_request(&context->upgrade, &upgrade_frame);
    if (result == DEVMGR_OK)
        result = devmgr_session_begin_request(&context->session, upgrade_frame.type,
                                              upgrade_frame.flags, upgrade_frame.payload,
                                              upgrade_frame.payload_length, policy,
                                              monotonic_ns(), &request);
    if (result == DEVMGR_OK) result = queue_frame(context, &request);
    if (result != DEVMGR_OK) devmgr_session_cancel_request(&context->session);
    return result;
}

static int read_transport(struct daemon_context *context)
{
    uint8_t input[2048];
    for (;;) {
        ssize_t count = devmgr_transport_read(&context->transport, input, sizeof(input));
        if (count > 0) {
            size_t emitted = 0U;
            int result = devmgr_parser_feed(&context->parser, input, (size_t)count, on_device_frame,
                                            context, &emitted);
            if (result != DEVMGR_OK) return result;
        } else if (count == 0) {
            /* A noncanonical TTY with VMIN=0/VTIME=0 may report no bytes after
             * the ready data has been drained. EPOLLHUP/ERR owns disconnect. */
            return DEVMGR_OK;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return DEVMGR_OK;
        } else {
            return DEVMGR_ERROR_DISCONNECTED;
        }
    }
}

static int start_request(struct daemon_context *context, size_t client_index,
                         const struct devmgr_ipc_request *ipc_request)
{
    struct devmgr_frame frame;
    struct devmgr_retry_policy policy = {.timeout_ms = 1000U, .retry_interval_ms = 500U,
                                         .max_retries = 2U};
    if (ipc_request->command == DEVMGR_IPC_GET_TELEMETRY) {
        if (context->latest_telemetry_length == 0U)
            return queue_client_response(context, client_index, ipc_request->command,
                                         DEVMGR_ERROR_NOT_FOUND, NULL, 0U);
        return queue_client_response(context, client_index, ipc_request->command, DEVMGR_OK,
                                     context->latest_telemetry,
                                     context->latest_telemetry_length);
    }
    if (ipc_request->command == DEVMGR_IPC_UPGRADE) {
        const uint8_t *path_end = memchr(ipc_request->payload, 0, ipc_request->payload_length);
        if (path_end == NULL) return queue_client_response(context, client_index,
                                                           ipc_request->command,
                                                           DEVMGR_ERROR_INVALID, NULL, 0U);
        size_t path_length = (size_t)(path_end - ipc_request->payload);
        size_t version_offset = path_length + 1U;
        if (path_length == 0U || version_offset >= ipc_request->payload_length ||
            memchr(ipc_request->payload + version_offset, 0,
                   ipc_request->payload_length - version_offset) == NULL)
            return queue_client_response(context, client_index, ipc_request->command,
                                         DEVMGR_ERROR_INVALID, NULL, 0U);
        if (context->session.state != DEVMGR_DEVICE_READY || context->active_client >= 0)
            return queue_client_response(context, client_index, ipc_request->command,
                                         DEVMGR_ERROR_BUSY, NULL, 0U);
        int upgrade_result = devmgr_firmware_open(
            &context->firmware, (const char *)ipc_request->payload);
        if (upgrade_result == DEVMGR_OK)
            upgrade_result = devmgr_upgrade_start(
                &context->upgrade, context->firmware.data, context->firmware.size,
                context->firmware.crc32, (const char *)ipc_request->payload + version_offset,
                DEVMGR_UPGRADE_DEFAULT_CHUNK);
        if (upgrade_result == DEVMGR_OK)
            upgrade_result = devmgr_session_transition(&context->session,
                                                       DEVMGR_SESSION_START_UPGRADE);
        if (upgrade_result != DEVMGR_OK) {
            devmgr_firmware_close(&context->firmware);
            return queue_client_response(context, client_index, ipc_request->command,
                                         upgrade_result, NULL, 0U);
        }
        context->active_client = (int)client_index;
        upgrade_result = queue_upgrade_request(context);
        if (upgrade_result != DEVMGR_OK) {
            context->active_client = -1;
            devmgr_firmware_close(&context->firmware);
            (void)devmgr_session_transition(&context->session, DEVMGR_SESSION_UPGRADE_DONE);
            return queue_client_response(context, client_index, ipc_request->command,
                                         upgrade_result, NULL, 0U);
        }
        return DEVMGR_OK;
    }
    if (ipc_request->command == DEVMGR_MSG_START_TELEMETRY &&
        context->session.state != DEVMGR_DEVICE_READY)
        return queue_client_response(context, client_index, ipc_request->command,
                                     DEVMGR_ERROR_STATE, NULL, 0U);
    if (ipc_request->command == DEVMGR_MSG_STOP_TELEMETRY &&
        context->session.state != DEVMGR_DEVICE_STREAMING)
        return queue_client_response(context, client_index, ipc_request->command,
                                     DEVMGR_ERROR_STATE, NULL, 0U);
    if (context->session.state != DEVMGR_DEVICE_READY &&
        context->session.state != DEVMGR_DEVICE_STREAMING) {
        return queue_client_response(context, client_index, ipc_request->command,
                                     DEVMGR_ERROR_STATE, NULL, 0U);
    }
    if (context->active_client >= 0) {
        return queue_client_response(context, client_index, ipc_request->command,
                                     DEVMGR_ERROR_BUSY, NULL, 0U);
    }
    int result = devmgr_session_begin_request(
        &context->session, ipc_request->command, DEVMGR_FRAME_ACK_REQUIRED, ipc_request->payload,
        (uint16_t)ipc_request->payload_length, policy, monotonic_ns(), &frame);
    if (result != DEVMGR_OK) {
        return queue_client_response(context, client_index, ipc_request->command, result, NULL, 0U);
    }
    result = queue_frame(context, &frame);
    if (result != DEVMGR_OK) {
        devmgr_session_cancel_request(&context->session);
        return queue_client_response(context, client_index, ipc_request->command, result, NULL, 0U);
    }
    context->active_client = (int)client_index;
    return DEVMGR_OK;
}

static int handle_client_input(struct daemon_context *context, size_t index)
{
    struct daemon_client *client = &context->clients[index];
    for (;;) {
        ssize_t count = read(client->fd, client->input + client->input_size,
                             sizeof(client->input) - client->input_size);
        if (count > 0) {
            client->input_size += (size_t)count;
            if (client->input_size >= DEVMGR_IPC_REQUEST_HEADER_SIZE && client->expected_size == 0U) {
                uint32_t payload_length = devmgr_get_le32(client->input + 4U);
                if (payload_length > DEVMGR_IPC_MAX_PAYLOAD) {
                    return queue_client_response(context, index, 0U, DEVMGR_ERROR_LIMIT, NULL, 0U);
                }
                client->expected_size = DEVMGR_IPC_REQUEST_HEADER_SIZE + payload_length;
            }
            if (client->expected_size != 0U && client->input_size == client->expected_size) {
                struct devmgr_ipc_request request;
                int result = devmgr_ipc_decode_request(client->input, client->input_size, &request);
                if (result != DEVMGR_OK) {
                    return queue_client_response(context, index, 0U, result, NULL, 0U);
                }
                return start_request(context, index, &request);
            }
        } else if (count == 0) {
            close_client(context, index);
            return DEVMGR_OK;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return DEVMGR_OK;
        } else if (errno != EINTR) {
            close_client(context, index);
            return DEVMGR_OK;
        }
    }
}

static int handle_client_output(struct daemon_context *context, size_t index)
{
    struct daemon_client *client = &context->clients[index];
    while (client->output_offset < client->output_size) {
        ssize_t count = write(client->fd, client->output + client->output_offset,
                              client->output_size - client->output_offset);
        if (count > 0) client->output_offset += (size_t)count;
        else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return DEVMGR_OK;
        else {
            close_client(context, index);
            return DEVMGR_OK;
        }
    }
    if (client->close_after_write) close_client(context, index);
    return DEVMGR_OK;
}

static int accept_clients(struct daemon_context *context)
{
    for (;;) {
        int fd = accept4(context->listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return DEVMGR_OK;
        if (fd < 0 && errno == EINTR) continue;
        if (fd < 0) return DEVMGR_ERROR_IO;
        size_t index;
        for (index = 0U; index < MAX_CLIENTS && context->clients[index].fd >= 0; ++index) {}
        if (index == MAX_CLIENTS) {
            (void)close(fd);
            continue;
        }
        memset(&context->clients[index], 0, sizeof(context->clients[index]));
        context->clients[index].fd = fd;
        if (epoll_update(context, EPOLL_CTL_ADD, fd, EPOLLIN | EPOLLRDHUP) != DEVMGR_OK) {
            close_client(context, index);
        }
    }
}

static int create_listener(const char *path)
{
    struct sockaddr_un address;
    struct stat info;
    int fd;
    size_t length = strlen(path);

    if (length == 0U || length >= sizeof(address.sun_path)) return -1;
    if (lstat(path, &info) == 0) {
        if (!S_ISSOCK(info.st_mode) || unlink(path) < 0) return -1;
    } else if (errno != ENOENT) return -1;
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path, length + 1U);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        chmod(path, S_IRUSR | S_IWUSR) < 0 || listen(fd, 16) < 0) {
        (void)close(fd);
        (void)unlink(path);
        return -1;
    }
    return fd;
}

static int setup_context(struct daemon_context *context, const struct devmgr_daemon_config *config)
{
    sigset_t signals;
    struct itimerspec interval = {.it_interval = {.tv_sec = 0, .tv_nsec = 100000000L},
                                  .it_value = {.tv_sec = 0, .tv_nsec = 100000000L}};
    struct devmgr_transport_config transport_config = {
        .kind = DEVMGR_TRANSPORT_SERIAL, .path = config->device_path, .baud_rate = 115200U};

    memset(context, 0, sizeof(*context));
    context->epoll_fd = context->listen_fd = context->timer_fd = context->signal_fd = -1;
    context->transport.fd = -1;
    context->firmware.fd = -1;
    context->active_client = -1;
    context->socket_path = config->socket_path;
    context->running = true;
    for (size_t index = 0U; index < MAX_CLIENTS; ++index) context->clients[index].fd = -1;
    devmgr_session_init(&context->session);
    devmgr_upgrade_init(&context->upgrade);
    if (devmgr_parser_init(&context->parser) != DEVMGR_OK) return DEVMGR_ERROR_IO;
    if (devmgr_transport_open(&context->transport, &transport_config) != DEVMGR_OK)
        return DEVMGR_ERROR_IO;
    context->listen_fd = create_listener(config->socket_path);
    context->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    context->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    (void)sigemptyset(&signals);
    (void)sigaddset(&signals, SIGINT);
    (void)sigaddset(&signals, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &signals, NULL) < 0) return DEVMGR_ERROR_IO;
    context->signal_fd = signalfd(-1, &signals, SFD_NONBLOCK | SFD_CLOEXEC);
    if (context->listen_fd < 0 || context->epoll_fd < 0 || context->timer_fd < 0 ||
        context->signal_fd < 0 || timerfd_settime(context->timer_fd, 0, &interval, NULL) < 0)
        return DEVMGR_ERROR_IO;
    if (epoll_update(context, EPOLL_CTL_ADD, context->listen_fd, EPOLLIN) != DEVMGR_OK ||
        epoll_update(context, EPOLL_CTL_ADD, context->timer_fd, EPOLLIN) != DEVMGR_OK ||
        epoll_update(context, EPOLL_CTL_ADD, context->signal_fd, EPOLLIN) != DEVMGR_OK ||
        epoll_update(context, EPOLL_CTL_ADD, devmgr_transport_get_fd(&context->transport),
                     EPOLLIN | EPOLLRDHUP) != DEVMGR_OK)
        return DEVMGR_ERROR_IO;
    return DEVMGR_OK;
}

static void cleanup_context(struct daemon_context *context)
{
    for (size_t index = 0U; index < MAX_CLIENTS; ++index) close_client(context, index);
    devmgr_transport_close(&context->transport);
    devmgr_firmware_close(&context->firmware);
    if (context->signal_fd >= 0) (void)close(context->signal_fd);
    if (context->timer_fd >= 0) (void)close(context->timer_fd);
    if (context->listen_fd >= 0) (void)close(context->listen_fd);
    if (context->epoll_fd >= 0) (void)close(context->epoll_fd);
    if (context->socket_path != NULL) (void)unlink(context->socket_path);
}

static int start_handshake(struct daemon_context *context)
{
    struct devmgr_frame frame;
    struct devmgr_retry_policy policy = {.timeout_ms = 1000U, .retry_interval_ms = 500U,
                                         .max_retries = 3U};
    int result = devmgr_session_transition(&context->session, DEVMGR_SESSION_CONNECT);
    if (result == DEVMGR_OK)
        result = devmgr_session_transition(&context->session, DEVMGR_SESSION_TRANSPORT_CONNECTED);
    if (result == DEVMGR_OK)
        result = devmgr_session_begin_request(&context->session, DEVMGR_MSG_PING,
                                              DEVMGR_FRAME_ACK_REQUIRED, NULL, 0U, policy,
                                              monotonic_ns(), &frame);
    return result == DEVMGR_OK ? queue_frame(context, &frame) : result;
}

int devmgr_daemon_run(const struct devmgr_daemon_config *config)
{
    struct daemon_context context;
    struct epoll_event events[32];
    int result;

    if (config == NULL || config->device_path == NULL || config->socket_path == NULL)
        return DEVMGR_ERROR_INVALID;
    result = setup_context(&context, config);
    if (result == DEVMGR_OK) result = start_handshake(&context);
    while (result == DEVMGR_OK && context.running) {
        int count = epoll_wait(context.epoll_fd, events, 32, -1);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { result = DEVMGR_ERROR_IO; break; }
        for (int event_index = 0; event_index < count && result == DEVMGR_OK; ++event_index) {
            int fd = events[event_index].data.fd;
            uint32_t flags = events[event_index].events;
            if (fd == context.listen_fd) result = accept_clients(&context);
            else if (fd == context.signal_fd) context.running = false;
            else if (fd == context.timer_fd) {
                uint64_t expirations;
                (void)read(context.timer_fd, &expirations, sizeof(expirations));
                result = devmgr_session_tick(&context.session, monotonic_ns(), retransmit_frame,
                                             &context);
                if (result == DEVMGR_ERROR_TIMEOUT && context.active_client >= 0) {
                    size_t index = (size_t)context.active_client;
                    if (devmgr_upgrade_active(&context.upgrade)) {
                        result = devmgr_upgrade_begin_recovery(&context.upgrade);
                        if (result == DEVMGR_OK) result = queue_upgrade_request(&context);
                        if (result == DEVMGR_OK) continue;
                    }
                    context.active_client = -1;
                    uint8_t command = devmgr_upgrade_active(&context.upgrade) ||
                                              context.upgrade.state == DEVMGR_UPGRADE_ERROR
                                          ? DEVMGR_IPC_UPGRADE : 0U;
                    if (command == DEVMGR_IPC_UPGRADE) {
                        context.upgrade.state = DEVMGR_UPGRADE_ERROR;
                        devmgr_firmware_close(&context.firmware);
                        (void)devmgr_session_transition(&context.session,
                                                       DEVMGR_SESSION_UPGRADE_DONE);
                    }
                    result = queue_client_response(&context, index, command,
                                                   DEVMGR_ERROR_TIMEOUT, NULL, 0U);
                }
            } else if (fd == devmgr_transport_get_fd(&context.transport)) {
                if ((flags & EPOLLOUT) != 0U) result = flush_transport(&context);
                if (result == DEVMGR_OK && (flags & EPOLLIN) != 0U) result = read_transport(&context);
                if ((flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0U) result = DEVMGR_ERROR_DISCONNECTED;
            } else {
                for (size_t index = 0U; index < MAX_CLIENTS; ++index) {
                    if (context.clients[index].fd != fd) continue;
                    if ((flags & EPOLLIN) != 0U) result = handle_client_input(&context, index);
                    if (result == DEVMGR_OK && context.clients[index].fd >= 0 &&
                        (flags & EPOLLOUT) != 0U) result = handle_client_output(&context, index);
                    if (context.clients[index].fd >= 0 &&
                        (flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0U)
                        close_client(&context, index);
                    break;
                }
            }
        }
    }
    if (result != DEVMGR_OK)
        devmgr_log_write(DEVMGR_LOG_ERROR, "daemon", "event loop: %s", devmgr_status_string(result));
    cleanup_context(&context);
    return result;
}
