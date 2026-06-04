/*
 * loxboot STM32 example — UART firmware update + dual-slot boot
 *
 * Target: STM32F4xx (internal flash, USART1)
 * HAL:    STM32CubeMX-generated HAL
 *
 * Memory layout (adjust to your specific device):
 *
 *   0x0800 0000 — Bootloader (this code, ≤ 16KB)
 *   0x0800 4000 — Boot state primary (one 16KB sector)
 *   0x0800 8000 — Boot state backup  (one 16KB sector)
 *   0x0802 0000 — Slot A firmware    (256KB)
 *   0x0806 0000 — Slot B firmware    (256KB)
 */

#include "main.h"               /* STM32CubeMX generated */
#include "usart.h"
#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"
#include "loxboot_flash_stm32.h"

/* -------------------------------------------------------------------------
 * UART adapter — wraps USART1 HAL into loxboot transport
 * ---------------------------------------------------------------------- */

static loxboot_err_t uart_read_byte(void *ctx, uint8_t *out, uint32_t timeout_ms)
{
    (void)ctx;
    HAL_StatusTypeDef r = HAL_UART_Receive(&huart1, out, 1, timeout_ms);
    return (r == HAL_OK) ? LOXBOOT_OK : LOXBOOT_ERR_TIMEOUT;
}

static loxboot_err_t uart_write_byte(void *ctx, uint8_t b)
{
    (void)ctx;
    HAL_StatusTypeDef r = HAL_UART_Transmit(&huart1, &b, 1, 10);
    return (r == HAL_OK) ? LOXBOOT_OK : LOXBOOT_ERR_TRANSPORT;
}

static loxboot_err_t uart_flush(void *ctx)
{
    (void)ctx;
    return LOXBOOT_OK;
}

/* -------------------------------------------------------------------------
 * Clock adapter
 * ---------------------------------------------------------------------- */

static uint32_t clock_now_ms(void *ctx)
{
    (void)ctx;
    return HAL_GetTick();
}

/* -------------------------------------------------------------------------
 * Fatal error handler — reset the device
 * ---------------------------------------------------------------------- */

static void on_fatal(void *ctx, loxboot_err_t reason)
{
    (void)ctx;
    (void)reason;
    NVIC_SystemReset();
}

/* -------------------------------------------------------------------------
 * Bootloader entry point
 * ---------------------------------------------------------------------- */

/* Place context in .noinit so app can call loxboot_confirm_boot() */
__attribute__((section(".noinit"))) static loxboot_t g_loxboot;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART1_UART_Init();

    /* Flash adapter */
    loxboot_stm32_flash_ctx_t flash_ctx = {0};
    loxboot_stm32_flash_adapter_init(&g_loxboot.flash, &flash_ctx);

    /* Clock adapter */
    g_loxboot.clock.now_ms = clock_now_ms;
    g_loxboot.clock.ctx    = NULL;

    /* UART transport adapter */
    g_loxboot.transport.read_byte  = uart_read_byte;
    g_loxboot.transport.write_byte = uart_write_byte;
    g_loxboot.transport.flush      = uart_flush;
    g_loxboot.transport.ctx        = NULL;

    /* Fatal error handler */
    g_loxboot.hal.on_fatal = on_fatal;
    g_loxboot.hal.ctx      = NULL;

    /* Flash memory layout — adjust to your linker script */
    g_loxboot.platform.boot_state_primary_base = 0x08004000UL;
    g_loxboot.platform.boot_state_backup_base  = 0x08008000UL;
    g_loxboot.platform.slot_a_base             = 0x08020000UL;
    g_loxboot.platform.slot_b_base             = 0x08060000UL;
    g_loxboot.platform.slot_size               = 0x00040000UL; /* 256 KB */

    if (loxboot_init(&g_loxboot) != LOXBOOT_OK) {
        on_fatal(NULL, LOXBOOT_ERR_INVALID_ARG);
    }

    /* Never returns — jumps to application or calls on_fatal */
    loxboot_run(&g_loxboot);
}

/* -------------------------------------------------------------------------
 * Application side (in your application firmware, not here)
 *
 * extern loxboot_t g_loxboot;   // same .noinit symbol
 *
 * void app_startup(void)
 * {
 *     // Call after watchdog init and self-test pass
 *     loxboot_confirm_boot(&g_loxboot);
 * }
 * ---------------------------------------------------------------------- */
