#include "devmgr/session.h"

#include "devmgr/error.h"

#include <limits.h>
#include <string.h>

static uint64_t milliseconds_to_ns(uint32_t milliseconds)
{
    return (uint64_t)milliseconds * UINT64_C(1000000);
}

static uint64_t saturating_add(uint64_t left, uint64_t right)
{
    return UINT64_MAX - left < right ? UINT64_MAX : left + right;
}

void devmgr_session_init(struct devmgr_session *session)
{
    if (session != NULL) {
        memset(session, 0, sizeof(*session));
        session->state = DEVMGR_DEVICE_DISCONNECTED;
        session->next_sequence = 1U;
    }
}

int devmgr_session_transition(struct devmgr_session *session, enum devmgr_session_event event)
{
    enum devmgr_device_state next;

    if (session == NULL) {
        return DEVMGR_ERROR_INVALID;
    }
    if (event == DEVMGR_SESSION_TRANSPORT_LOST || event == DEVMGR_SESSION_FAIL) {
        session->state = DEVMGR_DEVICE_DISCONNECTED;
        devmgr_session_cancel_request(session);
        return DEVMGR_OK;
    }
    next = session->state;
    switch (session->state) {
    case DEVMGR_DEVICE_DISCONNECTED:
        if (event == DEVMGR_SESSION_CONNECT) next = DEVMGR_DEVICE_CONNECTING;
        else return DEVMGR_ERROR_STATE;
        break;
    case DEVMGR_DEVICE_CONNECTING:
        if (event == DEVMGR_SESSION_TRANSPORT_CONNECTED) next = DEVMGR_DEVICE_HANDSHAKING;
        else return DEVMGR_ERROR_STATE;
        break;
    case DEVMGR_DEVICE_HANDSHAKING:
        if (event == DEVMGR_SESSION_HANDSHAKE_OK) next = DEVMGR_DEVICE_READY;
        else return DEVMGR_ERROR_STATE;
        break;
    case DEVMGR_DEVICE_READY:
        if (event == DEVMGR_SESSION_START_STREAM) next = DEVMGR_DEVICE_STREAMING;
        else if (event == DEVMGR_SESSION_START_UPGRADE) next = DEVMGR_DEVICE_UPGRADING;
        else if (event == DEVMGR_SESSION_REBOOT) next = DEVMGR_DEVICE_REBOOTING;
        else return DEVMGR_ERROR_STATE;
        break;
    case DEVMGR_DEVICE_STREAMING:
        if (event == DEVMGR_SESSION_STOP_STREAM) next = DEVMGR_DEVICE_READY;
        else if (event == DEVMGR_SESSION_REBOOT) next = DEVMGR_DEVICE_REBOOTING;
        else return DEVMGR_ERROR_STATE;
        break;
    case DEVMGR_DEVICE_UPGRADING:
        if (event == DEVMGR_SESSION_UPGRADE_DONE) next = DEVMGR_DEVICE_READY;
        else if (event == DEVMGR_SESSION_REBOOT) next = DEVMGR_DEVICE_REBOOTING;
        else return DEVMGR_ERROR_STATE;
        break;
    case DEVMGR_DEVICE_REBOOTING:
        if (event == DEVMGR_SESSION_TRANSPORT_CONNECTED) next = DEVMGR_DEVICE_HANDSHAKING;
        else return DEVMGR_ERROR_STATE;
        break;
    default: return DEVMGR_ERROR_STATE;
    }
    session->state = next;
    return DEVMGR_OK;
}

const char *devmgr_device_state_string(enum devmgr_device_state state)
{
    switch (state) {
    case DEVMGR_DEVICE_DISCONNECTED: return "DISCONNECTED";
    case DEVMGR_DEVICE_CONNECTING: return "CONNECTING";
    case DEVMGR_DEVICE_HANDSHAKING: return "HANDSHAKING";
    case DEVMGR_DEVICE_READY: return "READY";
    case DEVMGR_DEVICE_STREAMING: return "STREAMING";
    case DEVMGR_DEVICE_UPGRADING: return "UPGRADING";
    case DEVMGR_DEVICE_REBOOTING: return "REBOOTING";
    default: return "UNKNOWN";
    }
}

int devmgr_session_begin_request(struct devmgr_session *session, uint8_t type, uint8_t flags,
                                 const void *payload, uint16_t payload_length,
                                 struct devmgr_retry_policy policy, uint64_t now_ns,
                                 struct devmgr_frame *request)
{
    if (session == NULL || request == NULL || (payload == NULL && payload_length != 0U) ||
        payload_length > DEVMGR_PROTOCOL_MAX_PAYLOAD || policy.timeout_ms == 0U) {
        return DEVMGR_ERROR_INVALID;
    }
    if (session->pending.active) {
        return DEVMGR_ERROR_BUSY;
    }
    memset(request, 0, sizeof(*request));
    request->type = type;
    request->flags = flags;
    request->sequence = session->next_sequence++;
    if (session->next_sequence == 0U) {
        session->next_sequence = 1U;
    }
    request->payload_length = payload_length;
    if (payload_length != 0U) {
        memcpy(request->payload, payload, payload_length);
    }
    session->pending.frame = *request;
    session->pending.policy = policy;
    session->pending.deadline_ns = saturating_add(now_ns, milliseconds_to_ns(policy.timeout_ms));
    session->pending.retries_done = 0U;
    session->pending.active = true;
    ++session->stats.requests;
    return DEVMGR_OK;
}

int devmgr_session_accept_response(struct devmgr_session *session,
                                   const struct devmgr_frame *response)
{
    if (session == NULL || response == NULL) {
        return DEVMGR_ERROR_INVALID;
    }
    if (!session->pending.active) {
        if (response->sequence == session->last_completed_sequence) {
            ++session->stats.duplicate_responses;
        } else {
            ++session->stats.unexpected_responses;
        }
        return DEVMGR_ERROR_NOT_FOUND;
    }
    if (response->sequence != session->pending.frame.sequence) {
        ++session->stats.sequence_mismatches;
        return DEVMGR_ERROR_PROTOCOL;
    }
    if ((response->flags & DEVMGR_FRAME_RESPONSE) == 0U && response->type != DEVMGR_MSG_ACK &&
        response->type != DEVMGR_MSG_NACK && response->type != DEVMGR_MSG_ERROR) {
        ++session->stats.unexpected_responses;
        return DEVMGR_ERROR_PROTOCOL;
    }
    session->last_completed_sequence = response->sequence;
    session->pending.active = false;
    ++session->stats.responses;
    return DEVMGR_OK;
}

int devmgr_session_tick(struct devmgr_session *session, uint64_t now_ns,
                        devmgr_retransmit_callback retransmit, void *context)
{
    int result;
    uint64_t interval;

    if (session == NULL || retransmit == NULL) {
        return DEVMGR_ERROR_INVALID;
    }
    if (!session->pending.active || now_ns < session->pending.deadline_ns) {
        return DEVMGR_OK;
    }
    if (session->pending.retries_done >= session->pending.policy.max_retries) {
        session->pending.active = false;
        ++session->stats.timeouts;
        return DEVMGR_ERROR_TIMEOUT;
    }
    session->pending.frame.flags |= DEVMGR_FRAME_RETRY;
    result = retransmit(&session->pending.frame, context);
    if (result != DEVMGR_OK) {
        return result;
    }
    ++session->pending.retries_done;
    ++session->stats.retries;
    interval = milliseconds_to_ns(session->pending.policy.retry_interval_ms == 0U
                                      ? session->pending.policy.timeout_ms
                                      : session->pending.policy.retry_interval_ms);
    session->pending.deadline_ns = saturating_add(now_ns, interval);
    return DEVMGR_OK;
}

void devmgr_session_cancel_request(struct devmgr_session *session)
{
    if (session != NULL) {
        session->pending.active = false;
    }
}

