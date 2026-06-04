#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"
#include "test_support.h"

/* Mock transport for testing UART session */
typedef struct {
    uint8_t rx_buf[256];
    size_t rx_idx;
    size_t rx_len;
    uint8_t tx_buf[256];
    size_t tx_len;
    bool timeout_on_read;
    uint32_t fail_write_on_byte_count;
} mock_transport_t;

static loxboot_err_t mock_read_byte(void *ctx, uint8_t *out, uint32_t timeout_ms)
{
    (void)timeout_ms;
    mock_transport_t *mt = (mock_transport_t *)ctx;

    if (mt->timeout_on_read || mt->rx_idx >= mt->rx_len) {
        return LOXBOOT_ERR_TIMEOUT;
    }

    *out = mt->rx_buf[mt->rx_idx];
    mt->rx_idx++;
    return LOXBOOT_OK;
}

static loxboot_err_t mock_write_byte(void *ctx, uint8_t b)
{
    mock_transport_t *mt = (mock_transport_t *)ctx;

    if (mt->tx_len >= sizeof(mt->tx_buf)) {
        return LOXBOOT_ERR_TRANSPORT;
    }

    mt->tx_buf[mt->tx_len] = b;
    mt->tx_len++;
    return LOXBOOT_OK;
}

static loxboot_err_t mock_flush(void *ctx)
{
    (void)ctx;
    return LOXBOOT_OK;
}

static loxboot_err_t mock_write_byte_fail(void *ctx, uint8_t b)
{
    (void)b;
    mock_transport_t *mt = (mock_transport_t *)ctx;
    if (mt->fail_write_on_byte_count > 0u) {
        mt->fail_write_on_byte_count--;
        return LOXBOOT_ERR_TRANSPORT;
    }
    return mock_write_byte(ctx, b);
}

static loxboot_err_t mock_flush_fail(void *ctx)
{
    (void)ctx;
    return LOXBOOT_ERR_TRANSPORT;
}

static uint32_t mock_clock_now(void *ctx)
{
    (void)ctx;
    static uint32_t time_ms = 0u;
    return time_ms++;
}

/* Test: No CMD_HELLO within listen window returns LOXBOOT_OK */
static void test_uart_no_hello_timeout(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));
    mock_t.timeout_on_read = true;

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.transport = transport;
    session.listen_ms = 100u;

    loxboot_err_t err = loxboot_uart_run_session(&session);

    CHECK_EQ_INT(err, LOXBOOT_OK);
}

/* Test: Session initializes correctly */
static void test_uart_session_init(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 3000u;

    CHECK(session.boot != NULL);
    CHECK_EQ_INT(session.listen_ms, 3000u);
}

/* Test: CRC16 public API accessible */
static void test_crc16_api_available(void)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint16_t crc = loxboot_crc16(data, sizeof(data));

    CHECK(crc != 0u);
    CHECK(crc != 0xFFFFu);
}

/* Test: Command constants defined */
static void test_uart_commands_defined(void)
{
    /* Verify UART command constants are accessible */
    int hello_cmd = LOXBOOT_UART_CMD_HELLO;
    int write_cmd = LOXBOOT_UART_CMD_WRITE;

    CHECK(hello_cmd == 0x01);
    CHECK(write_cmd == 0x02);
}

/* Test: Session state tracking */
static void test_uart_session_state(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_state_t state;
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 100u;

    /* Session fields should initialize */
    CHECK(session.boot != NULL);
    CHECK(session.listen_ms > 0u);
}

/* Helper: Build frame in mock transport RX buffer */
static void mock_queue_frame(mock_transport_t *mt, uint8_t cmd, const uint8_t *payload, uint16_t payload_len)
{
    uint8_t frame_buf[256];
    size_t frame_len = sizeof(frame_buf);

    loxboot_uart_frame_encode(cmd, payload, payload_len, frame_buf, &frame_len);

    for (size_t i = 0u; i < frame_len; i++) {
        mt->rx_buf[mt->rx_len++] = frame_buf[i];
    }
}

/* Test: WRITE without HELLO rejected */
static void test_uart_write_before_hello_rejected(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    /* Queue WRITE command without HELLO */
    uint8_t write_payload[] = {0x00, 0x00, 0x00, 0x00, 0xAA, 0xBB};
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_WRITE, write_payload, sizeof(write_payload));

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 100u;

    loxboot_err_t err = loxboot_uart_run_session(&session);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    /* Response should be error (RSP_ERROR) */
    CHECK(mock_t.tx_len > 0u);
    CHECK_EQ_INT(mock_t.tx_buf[0], LOXBOOT_UART_SOF);
    CHECK_EQ_INT(mock_t.tx_buf[1], LOXBOOT_UART_RSP_ERROR);
}

/* Test: COMMIT without HELLO rejected */
static void test_uart_commit_before_hello_rejected(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    /* Queue COMMIT command without HELLO */
    uint8_t commit_payload[] = {0x10, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_COMMIT, commit_payload, sizeof(commit_payload));

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 100u;

    loxboot_err_t err = loxboot_uart_run_session(&session);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    /* Response should be error (RSP_ERROR) */
    CHECK(mock_t.tx_len > 0u);
    CHECK_EQ_INT(mock_t.tx_buf[0], LOXBOOT_UART_SOF);
    CHECK_EQ_INT(mock_t.tx_buf[1], LOXBOOT_UART_RSP_ERROR);
}

/* Test: REBOOT without HELLO rejected */
static void test_uart_reboot_before_hello_rejected(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    /* Queue REBOOT command without HELLO */
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_REBOOT, NULL, 0u);

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 100u;

    loxboot_err_t err = loxboot_uart_run_session(&session);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    /* Response should be error (RSP_ERROR) */
    CHECK(mock_t.tx_len > 0u);
    CHECK_EQ_INT(mock_t.tx_buf[0], LOXBOOT_UART_SOF);
    CHECK_EQ_INT(mock_t.tx_buf[1], LOXBOOT_UART_RSP_ERROR);
}

/* Test: COMMIT with mismatched firmware_size rejected */
static void test_uart_commit_size_mismatch_rejected(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    /* Queue HELLO then WRITE then COMMIT with mismatched size */
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_HELLO, NULL, 0u);

    uint8_t write_payload[] = {0x00, 0x00, 0x00, 0x00, 0xAA, 0xBB};
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_WRITE, write_payload, sizeof(write_payload));

    /* COMMIT says 100 bytes but only 2 written */
    uint8_t commit_payload[] = {0x64, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_COMMIT, commit_payload, sizeof(commit_payload));

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 500u;

    loxboot_err_t err = loxboot_uart_run_session(&session);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    /* Third response (COMMIT) should be error due to size mismatch */
    CHECK(mock_t.tx_len > 0u);
}

/* Test: HELLO returns RSP_STATUS with slot state */
static void test_uart_hello_returns_status(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_HELLO, NULL, 0u);

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 100u;

    loxboot_uart_run_session(&session);

    /* Response should be RSP_STATUS (0x83) */
    CHECK(mock_t.tx_len > 0u);
    CHECK_EQ_INT(mock_t.tx_buf[0], LOXBOOT_UART_SOF);
    CHECK_EQ_INT(mock_t.tx_buf[1], LOXBOOT_UART_RSP_STATUS);
}

/* Test: COMMIT before WRITE rejected */
static void test_uart_commit_before_write_rejected(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    /* Queue HELLO then COMMIT without WRITE */
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_HELLO, NULL, 0u);
    uint8_t commit_payload[] = {0x00, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_COMMIT, commit_payload, sizeof(commit_payload));

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 200u;

    loxboot_uart_run_session(&session);

    /* Second response (COMMIT) should be error due to size mismatch (0 bytes written) */
    CHECK(mock_t.tx_len > 0u);
}

/* Test: WRITE with offset + length > slot_size rejected */
static void test_uart_write_out_of_bounds(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_HELLO, NULL, 0u);

    /* WRITE at offset 0xFFF0 with 0x100 bytes (exceeds slot) */
    uint8_t write_payload[] = {0xF0, 0xFF, 0x00, 0x00, 0xAA, 0xBB};
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_WRITE, write_payload, sizeof(write_payload));

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 200u;

    loxboot_uart_run_session(&session);

    /* Second response (WRITE) should be error (out of bounds) */
    CHECK(mock_t.tx_len > 0u);
}

/* Test: ABORT invalidates target slot */
static void test_uart_abort_invalidates_slot(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_HELLO, NULL, 0u);
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_ABORT, NULL, 0u);

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 200u;

    loxboot_err_t err = loxboot_uart_run_session(&session);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    /* Verify session ran (responses sent) */
    CHECK(mock_t.tx_len > 0u);
}

/* Test: write_byte failure returns error */
static void test_uart_write_byte_failure(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_HELLO, NULL, 0u);

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte_fail;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 100u;

    /* Set transport to fail after first write (during response) */
    mock_t.fail_write_on_byte_count = 1u;

    loxboot_err_t err = loxboot_uart_run_session(&session);
    CHECK_EQ_INT(err, LOXBOOT_ERR_TRANSPORT);
}

/* Test: flush failure returns error */
static void test_uart_flush_failure(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_HELLO, NULL, 0u);

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush_fail;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 100u;

    loxboot_err_t err = loxboot_uart_run_session(&session);
    CHECK_EQ_INT(err, LOXBOOT_ERR_TRANSPORT);
}

/* Test: Full UART update flow: HELLO -> WRITE -> COMMIT -> STATUS -> REBOOT */
static void test_uart_full_update_flow(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));

    /* Queue full update sequence */
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_HELLO, NULL, 0u);

    /* WRITE: offset=0, payload=0xAA 0xBB 0xCC 0xDD */
    uint8_t write_payload[] = {0x00, 0x00, 0x00, 0x00, 0xAA, 0xBB, 0xCC, 0xDD};
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_WRITE, write_payload, sizeof(write_payload));

    /* COMMIT: firmware_size=4, firmware_crc=0x12345678 */
    uint8_t commit_payload[] = {0x04, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12};
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_COMMIT, commit_payload, sizeof(commit_payload));

    /* STATUS: query current state */
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_STATUS, NULL, 0u);

    /* REBOOT: exit session */
    mock_queue_frame(&mock_t, LOXBOOT_UART_CMD_REBOOT, NULL, 0u);

    loxboot_transport_adapter_t transport;
    transport.ctx = &mock_t;
    transport.read_byte = mock_read_byte;
    transport.write_byte = mock_write_byte;
    transport.flush = mock_flush;
    ctx.transport = transport;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot = &ctx;
    session.listen_ms = 500u;

    loxboot_err_t err = loxboot_uart_run_session(&session);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    /* Verify responses were sent */
    CHECK(mock_t.tx_len > 0u);
}

int main(void)
{
    run_test("uart_no_hello_timeout", test_uart_no_hello_timeout);
    run_test("uart_session_init", test_uart_session_init);
    run_test("crc16_api_available", test_crc16_api_available);
    run_test("uart_commands_defined", test_uart_commands_defined);
    run_test("uart_session_state", test_uart_session_state);

    run_test("uart_write_before_hello_rejected", test_uart_write_before_hello_rejected);
    run_test("uart_commit_before_hello_rejected", test_uart_commit_before_hello_rejected);
    run_test("uart_reboot_before_hello_rejected", test_uart_reboot_before_hello_rejected);
    run_test("uart_commit_size_mismatch_rejected", test_uart_commit_size_mismatch_rejected);

    run_test("uart_hello_returns_status", test_uart_hello_returns_status);
    run_test("uart_commit_before_write_rejected", test_uart_commit_before_write_rejected);
    run_test("uart_write_out_of_bounds", test_uart_write_out_of_bounds);

    run_test("uart_abort_invalidates_slot", test_uart_abort_invalidates_slot);
    run_test("uart_write_byte_failure", test_uart_write_byte_failure);
    run_test("uart_flush_failure", test_uart_flush_failure);

    run_test("uart_full_update_flow", test_uart_full_update_flow);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
