/*
 * loxboot generic example — custom flash + UART adapter
 *
 * Shows how to wire loxboot to any platform by implementing
 * the three adapter interfaces from scratch.
 *
 * This file compiles standalone (no vendor HAL required) and
 * serves as a template for porting to new hardware.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"

/* =========================================================================
 * Implement these six functions for your platform
 * ====================================================================== */

/* --- Flash adapter --------------------------------------------------------
 *
 * addr is an absolute flash address (as configured in loxboot_platform_t).
 * All three functions must be synchronous and return LOXBOOT_OK on success.
 */

static loxboot_err_t my_flash_read(void *ctx, uint32_t addr, uint8_t *buf, size_t len)
{
    (void)ctx;
    /* Example: memory-mapped flash (ARM Cortex-M, STM32, etc.) */
    memcpy(buf, (const void *)(uintptr_t)addr, len);
    return LOXBOOT_OK;
}

static loxboot_err_t my_flash_write(void *ctx, uint32_t addr, const uint8_t *buf, size_t len)
{
    (void)ctx;
    /* TODO: implement platform flash write */
    /* Must write len bytes from buf to flash address addr */
    /* Return LOXBOOT_ERR_FLASH_WRITE on failure */
    (void)addr;
    (void)buf;
    (void)len;
    return LOXBOOT_OK;
}

static loxboot_err_t my_flash_erase(void *ctx, uint32_t addr, size_t len)
{
    (void)ctx;
    /* TODO: implement platform flash erase */
    /* Must erase flash region [addr, addr+len).
     * IMPORTANT: platform must round len up to the next sector boundary.
     * loxboot may pass sizes smaller than one flash sector (e.g., 52 bytes
     * for boot state). The adapter is responsible for correct alignment. */
    (void)addr;
    (void)len;
    return LOXBOOT_OK;
}

/* --- UART transport adapter -----------------------------------------------
 *
 * read_byte  must block until one byte arrives or timeout_ms elapses.
 * write_byte must transmit one byte (may buffer internally).
 * flush      must drain the TX buffer before returning.
 */

static loxboot_err_t my_uart_read_byte(void *ctx, uint8_t *out, uint32_t timeout_ms)
{
    (void)ctx;
    /* TODO: implement platform UART receive with timeout */
    (void)timeout_ms;
    (void)out;
    return LOXBOOT_ERR_TIMEOUT; /* Replace with real implementation */
}

static loxboot_err_t my_uart_write_byte(void *ctx, uint8_t b)
{
    (void)ctx;
    /* TODO: implement platform UART transmit */
    (void)b;
    return LOXBOOT_OK;
}

static loxboot_err_t my_uart_flush(void *ctx)
{
    (void)ctx;
    /* TODO: wait until TX FIFO empty */
    return LOXBOOT_OK;
}

/* --- Clock adapter -------------------------------------------------------
 *
 * Returns monotonic milliseconds. Wrapping is allowed.
 */

static uint32_t my_clock_now_ms(void *ctx)
{
    (void)ctx;
    /* TODO: implement platform tick counter */
    return 0;
}

/* --- Fatal error handler -------------------------------------------------
 *
 * MUST NOT RETURN. Reset or halt the device.
 */

static void my_on_fatal(void *ctx, loxboot_err_t reason)
{
    (void)ctx;
    (void)reason;
    /* TODO: log reason, trigger watchdog reset, or spin */
    for (;;) {}
}

/* =========================================================================
 * Bootloader entry point
 * ====================================================================== */

static loxboot_t g_loxboot;

int main(void)
{
    /* Platform hardware init (clocks, UART, watchdog, etc.) */
    /* my_platform_init(); */

    memset(&g_loxboot, 0, sizeof(g_loxboot));

    /* Flash adapter */
    g_loxboot.flash.read  = my_flash_read;
    g_loxboot.flash.write = my_flash_write;
    g_loxboot.flash.erase = my_flash_erase;
    g_loxboot.flash.ctx   = NULL;

    /* UART transport adapter (set all three callbacks or loxboot rejects it) */
    g_loxboot.transport.read_byte  = my_uart_read_byte;
    g_loxboot.transport.write_byte = my_uart_write_byte;
    g_loxboot.transport.flush      = my_uart_flush;
    g_loxboot.transport.ctx        = NULL;

    /* Clock adapter */
    g_loxboot.clock.now_ms = my_clock_now_ms;
    g_loxboot.clock.ctx    = NULL;

    /* Fatal error handler */
    g_loxboot.hal.on_fatal = my_on_fatal;
    g_loxboot.hal.ctx      = NULL;

    /*
     * Flash memory layout — adjust addresses to match your linker script.
     *
     * Rules:
     *  - boot_state_primary and boot_state_backup must be in different sectors
     *  - slot_a and slot_b must not overlap each other or the boot state regions
     *  - slot_size must accommodate your largest expected firmware image
     *  - All addresses must be sector-erase-aligned for your platform
     */
    g_loxboot.platform.boot_state_primary_base = 0x08004000UL;
    g_loxboot.platform.boot_state_backup_base  = 0x08008000UL;
    g_loxboot.platform.slot_a_base             = 0x08020000UL;
    g_loxboot.platform.slot_b_base             = 0x08060000UL;
    g_loxboot.platform.slot_size               = 0x00040000UL; /* 256 KB */

    /* Validate all adapters and addresses */
    if (loxboot_init(&g_loxboot) != LOXBOOT_OK) {
        my_on_fatal(NULL, LOXBOOT_ERR_INVALID_ARG);
    }

    /*
     * Run the bootloader.
     *
     * Sequence:
     *  1. Read boot state from flash (dual-copy with CRC recovery)
     *  2. Listen on UART for LOXBOOT_UART_LISTEN_MS (default 3000ms)
     *     - If CMD_HELLO received: handle firmware update session
     *  3. Select slot to boot (ACTIVE or PENDING if rollback safe)
     *  4. Verify firmware CRC32
     *  5. Increment boot attempt counter, write to flash
     *  6. Jump to firmware entry point
     *
     * On any unrecoverable error: calls my_on_fatal() which must not return.
     */
    loxboot_run(&g_loxboot);

    /* Unreachable */
    return 0;
}

/* =========================================================================
 * Application side
 *
 * In your application firmware, call loxboot_confirm_boot() after all
 * startup self-tests pass. This resets the crash loop counter and marks
 * the active slot as confirmed (prevents rollback).
 *
 * If the app never calls confirm_boot(), loxboot increments boot_attempts
 * each boot. After LOXBOOT_MAX_BOOT_ATTEMPTS (default: 3) boots without
 * confirmation, loxboot rolls back to the previous slot.
 *
 * Example (application side):
 *
 *   extern loxboot_t g_loxboot;  // shared via .noinit RAM
 *
 *   void app_init_complete(void)
 *   {
 *       loxboot_confirm_boot(&g_loxboot);
 *   }
 * ====================================================================== */
