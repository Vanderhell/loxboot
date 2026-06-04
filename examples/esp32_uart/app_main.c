/*
 * loxboot ESP32 example — UART firmware update + dual-slot boot
 *
 * Target: ESP32 (ESP-IDF v5.x)
 *
 * Partition table (partitions.csv):
 *
 *   # Name,   Type, SubType, Offset,   Size
 *   nvs,      data, nvs,     0x9000,   0x6000
 *   phy_init, data, phy,     0xf000,   0x1000
 *   loxstate, data, 0x40,   0x10000,   0x1000   <- boot state primary
 *   loxbkup,  data, 0x41,   0x11000,   0x1000   <- boot state backup
 *   slot_a,   app,  ota_0,  0x20000,  0x180000  <- firmware slot A
 *   slot_b,   app,  ota_1,  0x1A0000, 0x180000  <- firmware slot B
 */

#include <string.h>
#include "esp_partition.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_system.h"

#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"
#include "loxboot_flash_esp32.h"

#define UART_PORT       UART_NUM_0
#define UART_BAUD       115200
#define UART_BUF_SIZE   2048

/* -------------------------------------------------------------------------
 * UART adapter
 * ---------------------------------------------------------------------- */

static loxboot_err_t uart_read_byte(void *ctx, uint8_t *out, uint32_t timeout_ms)
{
    (void)ctx;
    int r = uart_read_bytes(UART_PORT, out, 1, pdMS_TO_TICKS(timeout_ms));
    return (r == 1) ? LOXBOOT_OK : LOXBOOT_ERR_TIMEOUT;
}

static loxboot_err_t uart_write_byte(void *ctx, uint8_t b)
{
    (void)ctx;
    int r = uart_write_bytes(UART_PORT, &b, 1);
    return (r == 1) ? LOXBOOT_OK : LOXBOOT_ERR_TRANSPORT;
}

static loxboot_err_t uart_flush(void *ctx)
{
    (void)ctx;
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
    return LOXBOOT_OK;
}

/* -------------------------------------------------------------------------
 * Clock adapter
 * ---------------------------------------------------------------------- */

static uint32_t clock_now_ms(void *ctx)
{
    (void)ctx;
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* -------------------------------------------------------------------------
 * Fatal handler
 * ---------------------------------------------------------------------- */

static void on_fatal(void *ctx, loxboot_err_t reason)
{
    (void)ctx;
    (void)reason;
    esp_restart();
}

/* -------------------------------------------------------------------------
 * Flash adapter init — one adapter per partition
 * ---------------------------------------------------------------------- */

static void init_partition_adapter(loxboot_flash_adapter_t *out,
                                   loxboot_esp32_flash_ctx_t *ctx,
                                   const char *label)
{
    ctx->partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, label);
    loxboot_esp32_flash_adapter_init(out, ctx);
}

/* -------------------------------------------------------------------------
 * ESP32 app entry point
 * ---------------------------------------------------------------------- */

static loxboot_t g_loxboot;

/* One context per partition used as flash adapter */
static loxboot_esp32_flash_ctx_t g_state_ctx;
static loxboot_esp32_flash_ctx_t g_bkup_ctx;
static loxboot_esp32_flash_ctx_t g_slot_a_ctx;
static loxboot_esp32_flash_ctx_t g_slot_b_ctx;

void app_main(void)
{
    /* UART init */
    uart_config_t uart_cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_PORT, &uart_cfg);
    uart_driver_install(UART_PORT, UART_BUF_SIZE, 0, 0, NULL, 0);

    memset(&g_loxboot, 0, sizeof(g_loxboot));

    /* Flash adapters — one per partition */
    loxboot_flash_adapter_t state_flash, bkup_flash, slot_a_flash, slot_b_flash;
    init_partition_adapter(&state_flash,  &g_state_ctx,  "loxstate");
    init_partition_adapter(&bkup_flash,   &g_bkup_ctx,   "loxbkup");
    init_partition_adapter(&slot_a_flash, &g_slot_a_ctx, "slot_a");
    init_partition_adapter(&slot_b_flash, &g_slot_b_ctx, "slot_b");

    /*
     * ESP32 uses separate partitions per region — we use a single flash
     * adapter but route addresses via partition offsets in the adapter.
     * For simplicity, use slot_a_flash for the unified adapter and rely
     * on the adapter to resolve addresses relative to partition base.
     *
     * NOTE: Production integration should implement a multiplexing adapter
     * that routes reads/writes to the correct partition by address range.
     * See docs/PORTING.md for guidance.
     */
    g_loxboot.flash = slot_a_flash;

    /* Transport */
    g_loxboot.transport.read_byte  = uart_read_byte;
    g_loxboot.transport.write_byte = uart_write_byte;
    g_loxboot.transport.flush      = uart_flush;
    g_loxboot.transport.ctx        = NULL;

    /* Clock */
    g_loxboot.clock.now_ms = clock_now_ms;
    g_loxboot.clock.ctx    = NULL;

    /* Fatal */
    g_loxboot.hal.on_fatal = on_fatal;
    g_loxboot.hal.ctx      = NULL;

    /*
     * Platform layout — use partition base addresses.
     * Retrieve via esp_partition_get().
     */
    const esp_partition_t *pa = g_slot_a_ctx.partition;
    const esp_partition_t *pb = g_slot_b_ctx.partition;
    const esp_partition_t *ps = g_state_ctx.partition;
    const esp_partition_t *pb2 = g_bkup_ctx.partition;

    g_loxboot.platform.boot_state_primary_base = ps->address;
    g_loxboot.platform.boot_state_backup_base  = pb2->address;
    g_loxboot.platform.slot_a_base             = pa->address;
    g_loxboot.platform.slot_b_base             = pb->address;
    g_loxboot.platform.slot_size               = pa->size;

    if (loxboot_init(&g_loxboot) != LOXBOOT_OK) {
        on_fatal(NULL, LOXBOOT_ERR_INVALID_ARG);
    }

    loxboot_run(&g_loxboot);
}

/* -------------------------------------------------------------------------
 * Application side (in your application app_main):
 *
 * extern loxboot_t g_loxboot;
 *
 * void app_startup_complete(void)
 * {
 *     loxboot_confirm_boot(&g_loxboot);
 * }
 * ---------------------------------------------------------------------- */
