#include "test.h"

#include "devmgr/error.h"
#include "devmgr/session.h"

#include <stdint.h>
#include <string.h>

static int retransmit_count(const struct devmgr_frame *frame, void *context)
{
    unsigned *count = context;
    TEST_CHECK((frame->flags & DEVMGR_FRAME_RETRY) != 0U);
    ++*count;
    return DEVMGR_OK;
}

static int test_state_machine(void)
{
    struct devmgr_session session;
    devmgr_session_init(&session);
    TEST_CHECK(session.state == DEVMGR_DEVICE_DISCONNECTED);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_HANDSHAKE_OK) ==
               DEVMGR_ERROR_STATE);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_CONNECT) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_TRANSPORT_CONNECTED) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_HANDSHAKE_OK) == DEVMGR_OK);
    TEST_CHECK(session.state == DEVMGR_DEVICE_READY);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_START_STREAM) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_STOP_STREAM) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_START_UPGRADE) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_REBOOT) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_TRANSPORT_CONNECTED) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_HANDSHAKE_OK) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_transition(&session, DEVMGR_SESSION_TRANSPORT_LOST) == DEVMGR_OK);
    TEST_CHECK(session.state == DEVMGR_DEVICE_DISCONNECTED);
    return 0;
}

static int test_request_response(void)
{
    struct devmgr_session session;
    struct devmgr_frame request;
    struct devmgr_frame response = {0};
    const uint8_t payload[] = {1U, 2U};
    struct devmgr_retry_policy policy = {.timeout_ms = 100U, .retry_interval_ms = 50U,
                                         .max_retries = 2U};

    devmgr_session_init(&session);
    TEST_CHECK(devmgr_session_begin_request(&session, DEVMGR_MSG_PING, 0U, payload,
                                            sizeof(payload), policy, 1000U, &request) == DEVMGR_OK);
    TEST_CHECK(request.sequence == 1U && session.pending.active);
    TEST_CHECK(devmgr_session_begin_request(&session, DEVMGR_MSG_PING, 0U, NULL, 0U, policy, 0U,
                                            &request) == DEVMGR_ERROR_BUSY);
    response.type = DEVMGR_MSG_PING;
    response.flags = DEVMGR_FRAME_RESPONSE;
    response.sequence = 99U;
    TEST_CHECK(devmgr_session_accept_response(&session, &response) == DEVMGR_ERROR_PROTOCOL);
    response.sequence = 1U;
    TEST_CHECK(devmgr_session_accept_response(&session, &response) == DEVMGR_OK);
    TEST_CHECK(!session.pending.active && session.stats.responses == 1U);
    TEST_CHECK(devmgr_session_accept_response(&session, &response) == DEVMGR_ERROR_NOT_FOUND);
    TEST_CHECK(session.stats.duplicate_responses == 1U);
    return 0;
}

static int test_retry_timeout(void)
{
    struct devmgr_session session;
    struct devmgr_frame request;
    struct devmgr_retry_policy policy = {.timeout_ms = 1U, .retry_interval_ms = 2U,
                                         .max_retries = 2U};
    unsigned retries = 0U;

    devmgr_session_init(&session);
    TEST_CHECK(devmgr_session_begin_request(&session, DEVMGR_MSG_GET_INFO, 0U, NULL, 0U, policy,
                                            0U, &request) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_tick(&session, 999999U, retransmit_count, &retries) == DEVMGR_OK);
    TEST_CHECK(retries == 0U);
    TEST_CHECK(devmgr_session_tick(&session, 1000000U, retransmit_count, &retries) == DEVMGR_OK);
    TEST_CHECK(devmgr_session_tick(&session, 3000000U, retransmit_count, &retries) == DEVMGR_OK);
    TEST_CHECK(retries == 2U && session.stats.retries == 2U);
    TEST_CHECK(devmgr_session_tick(&session, 5000000U, retransmit_count, &retries) ==
               DEVMGR_ERROR_TIMEOUT);
    TEST_CHECK(!session.pending.active && session.stats.timeouts == 1U);
    return 0;
}

int main(void)
{
    int failed = 0;
    TEST_RUN(test_state_machine);
    TEST_RUN(test_request_response);
    TEST_RUN(test_retry_timeout);
    return failed == 0 ? 0 : 1;
}

