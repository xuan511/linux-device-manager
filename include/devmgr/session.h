#ifndef DEVMGR_SESSION_H
#define DEVMGR_SESSION_H

#include "devmgr/protocol.h"

#include <stdbool.h>
#include <stdint.h>

enum devmgr_device_state {
    DEVMGR_DEVICE_DISCONNECTED = 0,
    DEVMGR_DEVICE_CONNECTING,
    DEVMGR_DEVICE_HANDSHAKING,
    DEVMGR_DEVICE_READY,
    DEVMGR_DEVICE_STREAMING,
    DEVMGR_DEVICE_UPGRADING,
    DEVMGR_DEVICE_REBOOTING
};

enum devmgr_session_event {
    DEVMGR_SESSION_CONNECT = 0,
    DEVMGR_SESSION_TRANSPORT_CONNECTED,
    DEVMGR_SESSION_HANDSHAKE_OK,
    DEVMGR_SESSION_START_STREAM,
    DEVMGR_SESSION_STOP_STREAM,
    DEVMGR_SESSION_START_UPGRADE,
    DEVMGR_SESSION_UPGRADE_DONE,
    DEVMGR_SESSION_REBOOT,
    DEVMGR_SESSION_TRANSPORT_LOST,
    DEVMGR_SESSION_FAIL
};

struct devmgr_retry_policy {
    uint32_t timeout_ms;
    uint32_t retry_interval_ms;
    uint8_t max_retries;
};

struct devmgr_session_stats {
    uint64_t requests;
    uint64_t responses;
    uint64_t retries;
    uint64_t timeouts;
    uint64_t sequence_mismatches;
    uint64_t duplicate_responses;
    uint64_t unexpected_responses;
};

struct devmgr_pending_request {
    struct devmgr_frame frame;
    struct devmgr_retry_policy policy;
    uint64_t deadline_ns;
    uint8_t retries_done;
    bool active;
};

struct devmgr_session {
    enum devmgr_device_state state;
    uint32_t next_sequence;
    uint32_t last_completed_sequence;
    struct devmgr_pending_request pending;
    struct devmgr_session_stats stats;
};

typedef int (*devmgr_retransmit_callback)(const struct devmgr_frame *frame, void *context);

void devmgr_session_init(struct devmgr_session *session);
int devmgr_session_transition(struct devmgr_session *session, enum devmgr_session_event event);
const char *devmgr_device_state_string(enum devmgr_device_state state);
int devmgr_session_begin_request(struct devmgr_session *session, uint8_t type, uint8_t flags,
                                 const void *payload, uint16_t payload_length,
                                 struct devmgr_retry_policy policy, uint64_t now_ns,
                                 struct devmgr_frame *request);
int devmgr_session_accept_response(struct devmgr_session *session,
                                   const struct devmgr_frame *response);
int devmgr_session_tick(struct devmgr_session *session, uint64_t now_ns,
                        devmgr_retransmit_callback retransmit, void *context);
void devmgr_session_cancel_request(struct devmgr_session *session);

#endif

