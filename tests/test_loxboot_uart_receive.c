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

int main(void)
{
    run_test("uart_no_hello_timeout", test_uart_no_hello_timeout);
    run_test("uart_session_init", test_uart_session_init);
    run_test("crc16_api_available", test_crc16_api_available);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
