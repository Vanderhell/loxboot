#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "loxboot_uart.h"
#include "test_support.h"

/* Mock transport adapter with byte buffer */
typedef struct {
    uint8_t tx_buf[2048];
    size_t tx_len;
    uint8_t rx_buf[2048];
    size_t rx_idx;
    size_t rx_len;
    bool timeout_on_read;
} mock_transport_t;

static loxboot_err_t mock_transport_read(void *ctx, uint8_t *out, uint32_t timeout_ms)
{
    (void)timeout_ms;
    mock_transport_t *mt = (mock_transport_t *)ctx;

    if (mt->timeout_on_read) {
        return LOXBOOT_ERR_TIMEOUT;
    }

    if (mt->rx_idx >= mt->rx_len) {
        return LOXBOOT_ERR_TIMEOUT;
    }

    *out = mt->rx_buf[mt->rx_idx];
    mt->rx_idx++;
    return LOXBOOT_OK;
}

static loxboot_err_t mock_transport_write(void *ctx, uint8_t b)
{
    mock_transport_t *mt = (mock_transport_t *)ctx;

    if (mt->tx_len >= sizeof(mt->tx_buf)) {
        return LOXBOOT_ERR_TRANSPORT;
    }

    mt->tx_buf[mt->tx_len] = b;
    mt->tx_len++;
    return LOXBOOT_OK;
}

static loxboot_err_t mock_transport_flush(void *ctx)
{
    (void)ctx;
    return LOXBOOT_OK;
}

static uint32_t mock_clock_now(void *ctx)
{
    (void)ctx;
    static uint32_t time_ms = 0u;
    return time_ms++;
}

/* No CMD_HELLO within listen window */
static void test_uart_no_hello(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    mock_transport_t mock_t;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    memset(&mock_t, 0, sizeof(mock_t));
    ctx.transport.ctx = &mock_t;
    ctx.transport.read_byte = mock_transport_read;
    ctx.transport.write_byte = mock_transport_write;
    ctx.transport.flush = mock_transport_flush;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    mock_t.timeout_on_read = true;

    loxboot_uart_session_t session;
    loxboot_err_t err = loxboot_uart_run(&ctx, &session);

    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(mock_t.tx_len, 0u);
}

/* CMD_HELLO → RSP_STATUS */
static void test_uart_hello_response(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;
    mock_transport_t mock_t;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    test_build_default_state(&state, LOXBOOT_SLOT_A);
    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    state.slots[1].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[1], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));
    test_seed_state(&flash, &ctx.platform, &state);

    memset(&mock_t, 0, sizeof(mock_t));
    ctx.transport.ctx = &mock_t;
    ctx.transport.read_byte = mock_transport_read;
    ctx.transport.write_byte = mock_transport_write;
    ctx.transport.flush = mock_transport_flush;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    uint8_t hello_frame[] = {
        LOXBOOT_UART_SOF,
        LOXBOOT_UART_CMD_HELLO,
        0x00, 0x00,
        0x00, 0x00
    };

    uint16_t crc = loxboot_crc16(&hello_frame[1], 3u);
    hello_frame[4] = (uint8_t)(crc & 0xFFU);
    hello_frame[5] = (uint8_t)((crc >> 8) & 0xFFU);

    mock_t.rx_buf[0] = LOXBOOT_UART_SOF;
    mock_t.rx_buf[1] = LOXBOOT_UART_CMD_HELLO;
    mock_t.rx_buf[2] = 0x00;
    mock_t.rx_buf[3] = 0x00;
    mock_t.rx_buf[4] = hello_frame[4];
    mock_t.rx_buf[5] = hello_frame[5];
    mock_t.rx_len = 6u;

    loxboot_uart_session_t session;
    loxboot_err_t err = loxboot_uart_run(&ctx, &session);

    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK(mock_t.tx_len > 0u);
    CHECK_EQ_INT(mock_t.tx_buf[0], LOXBOOT_UART_SOF);
    CHECK_EQ_INT(mock_t.tx_buf[1], LOXBOOT_UART_RSP_STATUS);
}

/* CMD_ABORT → invalidate slot */
static void test_uart_abort(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;
    mock_transport_t mock_t;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    test_build_default_state(&state, LOXBOOT_SLOT_A);
    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    state.slots[1].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[1], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));
    test_seed_state(&flash, &ctx.platform, &state);

    memset(&mock_t, 0, sizeof(mock_t));
    ctx.transport.ctx = &mock_t;
    ctx.transport.read_byte = mock_transport_read;
    ctx.transport.write_byte = mock_transport_write;
    ctx.transport.flush = mock_transport_flush;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    uint8_t hello_frame[] = {LOXBOOT_UART_SOF, LOXBOOT_UART_CMD_HELLO, 0x00, 0x00, 0x00, 0x00};
    uint16_t crc = loxboot_crc16(&hello_frame[1], 3u);
    hello_frame[4] = (uint8_t)(crc & 0xFFU);
    hello_frame[5] = (uint8_t)((crc >> 8) & 0xFFU);

    uint8_t abort_frame[] = {LOXBOOT_UART_SOF, LOXBOOT_UART_CMD_ABORT, 0x00, 0x00, 0x00, 0x00};
    crc = loxboot_crc16(&abort_frame[1], 3u);
    abort_frame[4] = (uint8_t)(crc & 0xFFU);
    abort_frame[5] = (uint8_t)((crc >> 8) & 0xFFU);

    for (int i = 0; i < 6; i++) {
        mock_t.rx_buf[i] = hello_frame[i];
        mock_t.rx_buf[6 + i] = abort_frame[i];
    }
    mock_t.rx_len = 12u;

    loxboot_uart_session_t session;
    loxboot_err_t err = loxboot_uart_run(&ctx, &session);

    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK(mock_t.tx_len > 0u);
}

/* CMD_WRITE out of bounds → RSP_ERROR */
static void test_uart_write_out_of_bounds(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;
    mock_transport_t mock_t;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    test_build_default_state(&state, LOXBOOT_SLOT_A);
    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    state.slots[1].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[1], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));
    test_seed_state(&flash, &ctx.platform, &state);

    memset(&mock_t, 0, sizeof(mock_t));
    ctx.transport.ctx = &mock_t;
    ctx.transport.read_byte = mock_transport_read;
    ctx.transport.write_byte = mock_transport_write;
    ctx.transport.flush = mock_transport_flush;

    loxboot_clock_adapter_t clock;
    clock.ctx = NULL;
    clock.now_ms = mock_clock_now;
    ctx.clock = clock;

    uint8_t hello_frame[] = {LOXBOOT_UART_SOF, LOXBOOT_UART_CMD_HELLO, 0x00, 0x00, 0x00, 0x00};
    uint16_t crc = loxboot_crc16(&hello_frame[1], 3u);
    hello_frame[4] = (uint8_t)(crc & 0xFFU);
    hello_frame[5] = (uint8_t)((crc >> 8) & 0xFFU);

    uint8_t write_frame[] = {
        LOXBOOT_UART_SOF,
        LOXBOOT_UART_CMD_WRITE,
        0x0Du, 0x00,
        0xFF, 0xFF, 0xFF, 0x7F,
        0x00, 0x01, 0x02, 0x03,
        0x00, 0x00
    };
    crc = loxboot_crc16(&write_frame[1], 11u);
    write_frame[12] = (uint8_t)(crc & 0xFFU);
    write_frame[13] = (uint8_t)((crc >> 8) & 0xFFU);

    for (int i = 0; i < 6; i++) {
        mock_t.rx_buf[i] = hello_frame[i];
    }
    for (int i = 0; i < 14; i++) {
        mock_t.rx_buf[6 + i] = write_frame[i];
    }
    mock_t.rx_len = 20u;

    loxboot_uart_session_t session;
    loxboot_err_t err = loxboot_uart_run(&ctx, &session);

    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK(mock_t.tx_len > 0u);
}

int main(void)
{
    run_test("uart_no_hello", test_uart_no_hello);
    run_test("uart_hello_response", test_uart_hello_response);
    run_test("uart_abort", test_uart_abort);
    run_test("uart_write_out_of_bounds", test_uart_write_out_of_bounds);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
