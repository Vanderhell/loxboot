/*
 * loxboot_sim — UART device simulator for end-to-end testing.
 *
 * Implements the loxboot device side over stdin/stdout (binary).
 * The host (test_e2e.py) sends loxboot UART frames; this process responds.
 *
 * Usage: loxboot_sim [--scenario <name>]
 *   default   — slot A active, slot B empty (fresh device)
 *   pending_b — slot B already PENDING (simulates post-update state)
 *
 * Exit codes:
 *   0 — session ended cleanly (REBOOT or timeout)
 *   1 — session error or fatal
 *
 * State is logged to stderr as key=value lines for test_e2e.py to parse.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"
#include "test_support.h"

extern loxboot_err_t loxboot_state_read(loxboot_t *ctx, loxboot_state_t *out_state);

/* -------------------------------------------------------------------------
 * Global state
 * ---------------------------------------------------------------------- */

static test_flash_t g_flash;
static test_fatal_t g_fatal;

/* -------------------------------------------------------------------------
 * Transport adapter — stdin/stdout
 * ---------------------------------------------------------------------- */

static loxboot_err_t sim_read_byte(void *ctx, uint8_t *out, uint32_t timeout_ms)
{
    (void)ctx;
    (void)timeout_ms;
    int c = fgetc(stdin);
    if (c == EOF) {
        return LOXBOOT_ERR_TIMEOUT;
    }
    *out = (uint8_t)c;
    return LOXBOOT_OK;
}

static loxboot_err_t sim_write_byte(void *ctx, uint8_t b)
{
    (void)ctx;
    if (fputc(b, stdout) == EOF) {
        return LOXBOOT_ERR_TRANSPORT;
    }
    return LOXBOOT_OK;
}

static loxboot_err_t sim_flush(void *ctx)
{
    (void)ctx;
    fflush(stdout);
    return LOXBOOT_OK;
}

/* -------------------------------------------------------------------------
 * Clock adapter — frozen at 0 so listen window never expires
 * ---------------------------------------------------------------------- */

static uint32_t sim_clock_now_ms(void *ctx)
{
    (void)ctx;
    return 0u;
}

/* -------------------------------------------------------------------------
 * Fatal handler
 * ---------------------------------------------------------------------- */

static void sim_on_fatal(void *ctx, loxboot_err_t reason)
{
    (void)ctx;
    fprintf(stderr, "fatal=%d\n", (int)reason);
    exit(1);
}

/* -------------------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------------- */

int main(int argc, char **argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    const char *scenario = "default";
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--scenario") == 0) {
            scenario = argv[i + 1];
        }
    }

    /* Set up virtual flash */
    test_flash_reset(&g_flash);
    g_fatal.count = 0;

    /* Build loxboot context */
    loxboot_t ctx;
    test_make_valid_ctx(&ctx, &g_flash, &g_fatal);

    /* Override adapters for simulator */
    ctx.clock.now_ms       = sim_clock_now_ms;
    ctx.clock.ctx          = NULL;
    ctx.transport.read_byte  = sim_read_byte;
    ctx.transport.write_byte = sim_write_byte;
    ctx.transport.flush      = sim_flush;
    ctx.transport.ctx        = NULL;
    ctx.hal.on_fatal         = sim_on_fatal;
    ctx.hal.ctx              = NULL;

    /* Seed boot state based on scenario */
    loxboot_state_t init_state;
    test_build_default_state(&init_state, LOXBOOT_SLOT_A);

    if (strcmp(scenario, "pending_b") == 0) {
        /* Simulate: slot B written and COMMIT done, not yet booted */
        init_state.slots[LOXBOOT_SLOT_B].state          = LOXBOOT_SLOT_STATE_PENDING;
        init_state.slots[LOXBOOT_SLOT_B].firmware_size  = 4u;
        init_state.slots[LOXBOOT_SLOT_B].firmware_crc32 = 0xDEBB9EC5u; /* crc32 of 0xAA 0xBB 0xCC 0xDD */
    }

    test_seed_state(&g_flash, &ctx.platform, &init_state);

    loxboot_err_t err = loxboot_init(&ctx);
    if (err != LOXBOOT_OK) {
        fprintf(stderr, "init_err=%d\n", (int)err);
        return 1;
    }

    /* Run UART session */
    loxboot_uart_session_t session;
    memset(&session, 0, sizeof(session));
    session.boot       = &ctx;
    session.listen_ms  = 30000u; /* 30s nominal; clock frozen so no expiry */

    err = loxboot_uart_run_session(&session);

    /* Report final state to stderr for test_e2e.py to parse */
    loxboot_state_t final_state;
    (void)loxboot_state_read(&ctx, &final_state);

    fprintf(stderr, "session_err=%d\n",        (int)err);
    fprintf(stderr, "bytes_written=%u\n",       (unsigned)session._bytes_written);
    fprintf(stderr, "slot_erased=%d\n",         (int)session._slot_erased);
    fprintf(stderr, "slot_a_state=%d\n",        (int)final_state.slots[LOXBOOT_SLOT_A].state);
    fprintf(stderr, "slot_b_state=%d\n",        (int)final_state.slots[LOXBOOT_SLOT_B].state);
    fprintf(stderr, "slot_b_fw_size=%u\n",      (unsigned)final_state.slots[LOXBOOT_SLOT_B].firmware_size);
    fprintf(stderr, "slot_b_fw_crc=0x%08X\n",  final_state.slots[LOXBOOT_SLOT_B].firmware_crc32);
    fprintf(stderr, "active_slot=%d\n",         (int)final_state.active_slot);

    return (err == LOXBOOT_OK) ? 0 : 1;
}
