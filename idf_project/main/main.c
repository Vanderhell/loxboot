#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"

#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"
#include "loxboot_flash_esp32.h"

static const char *TAG = "loxboot";

#define USB_BUF 4096

/* -------------------------------------------------------------------------
 * Transport adapter — USB Serial JTAG (COM19 on host)
 * ---------------------------------------------------------------------- */
static loxboot_err_t usb_read_byte(void *ctx, uint8_t *out, uint32_t timeout_ms)
{
    (void)ctx;
    return (usb_serial_jtag_read_bytes(out, 1, pdMS_TO_TICKS(timeout_ms)) == 1)
           ? LOXBOOT_OK : LOXBOOT_ERR_TIMEOUT;
}

static loxboot_err_t usb_write_byte(void *ctx, uint8_t b)
{
    (void)ctx;
    return (usb_serial_jtag_write_bytes(&b, 1, pdMS_TO_TICKS(100)) == 1)
           ? LOXBOOT_OK : LOXBOOT_ERR_TRANSPORT;
}

static loxboot_err_t lox_uart_flush(void *ctx)
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
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* -------------------------------------------------------------------------
 * Fatal handler
 * ---------------------------------------------------------------------- */
static void on_fatal(void *ctx, loxboot_err_t reason)
{
    (void)ctx;
    ESP_LOGE(TAG, "FATAL: err=%d — restarting", (int)reason);
    esp_restart();
}

/* -------------------------------------------------------------------------
 * Flash adapter — single NVS-style adapter routing by address
 *
 * ESP32 has separate partitions; we build one unified adapter using the
 * loxboot_flash_esp32 adapter per partition and route by address range.
 * ---------------------------------------------------------------------- */

typedef struct {
    loxboot_flash_adapter_t  state_primary;
    loxboot_flash_adapter_t  state_backup;
    loxboot_flash_adapter_t  slot_a;
    loxboot_flash_adapter_t  slot_b;
    loxboot_esp32_flash_ctx_t ctx_sp;
    loxboot_esp32_flash_ctx_t ctx_sb;
    loxboot_esp32_flash_ctx_t ctx_sa;
    loxboot_esp32_flash_ctx_t ctx_sbb;
    uint32_t sp_base, sb_base, sa_base, sbb_base;
    size_t   sp_size, sb_size, sa_size, sbb_size;
} multi_flash_t;

static multi_flash_t g_mflash;

static loxboot_err_t mflash_read(void *ctx, uint32_t addr, uint8_t *buf, size_t len)
{
    multi_flash_t *m = (multi_flash_t *)ctx;
    if (addr >= m->sp_base  && addr + len <= m->sp_base  + m->sp_size)
        return m->state_primary.read(m->state_primary.ctx,  addr - m->sp_base,  buf, len);
    if (addr >= m->sb_base  && addr + len <= m->sb_base  + m->sb_size)
        return m->state_backup.read(m->state_backup.ctx,   addr - m->sb_base,  buf, len);
    if (addr >= m->sa_base  && addr + len <= m->sa_base  + m->sa_size)
        return m->slot_a.read(m->slot_a.ctx,               addr - m->sa_base,  buf, len);
    if (addr >= m->sbb_base && addr + len <= m->sbb_base + m->sbb_size)
        return m->slot_b.read(m->slot_b.ctx,               addr - m->sbb_base, buf, len);
    return LOXBOOT_ERR_FLASH_READ;
}

static loxboot_err_t mflash_write(void *ctx, uint32_t addr, const uint8_t *buf, size_t len)
{
    multi_flash_t *m = (multi_flash_t *)ctx;
    if (addr >= m->sp_base  && addr + len <= m->sp_base  + m->sp_size)
        return m->state_primary.write(m->state_primary.ctx,  addr - m->sp_base,  buf, len);
    if (addr >= m->sb_base  && addr + len <= m->sb_base  + m->sb_size)
        return m->state_backup.write(m->state_backup.ctx,   addr - m->sb_base,  buf, len);
    if (addr >= m->sa_base  && addr + len <= m->sa_base  + m->sa_size)
        return m->slot_a.write(m->slot_a.ctx,               addr - m->sa_base,  buf, len);
    if (addr >= m->sbb_base && addr + len <= m->sbb_base + m->sbb_size)
        return m->slot_b.write(m->slot_b.ctx,               addr - m->sbb_base, buf, len);
    return LOXBOOT_ERR_FLASH_WRITE;
}

static loxboot_err_t mflash_erase(void *ctx, uint32_t addr, size_t len)
{
    multi_flash_t *m = (multi_flash_t *)ctx;
    if (addr >= m->sp_base  && addr + len <= m->sp_base  + m->sp_size)
        return m->state_primary.erase(m->state_primary.ctx,  addr - m->sp_base,  len);
    if (addr >= m->sb_base  && addr + len <= m->sb_base  + m->sb_size)
        return m->state_backup.erase(m->state_backup.ctx,   addr - m->sb_base,  len);
    if (addr >= m->sa_base  && addr + len <= m->sa_base  + m->sa_size)
        return m->slot_a.erase(m->slot_a.ctx,               addr - m->sa_base,  len);
    if (addr >= m->sbb_base && addr + len <= m->sbb_base + m->sbb_size)
        return m->slot_b.erase(m->slot_b.ctx,               addr - m->sbb_base, len);
    return LOXBOOT_ERR_FLASH_ERASE;
}

/* -------------------------------------------------------------------------
 * Bootloader entry point
 * ---------------------------------------------------------------------- */
static loxboot_t g_loxboot;

void app_main(void)
{
    /* USB Serial JTAG init — this is COM19 on host */
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = USB_BUF,
        .tx_buffer_size = USB_BUF,
    };
    usb_serial_jtag_driver_install(&usb_cfg);

    ESP_LOGI(TAG, "loxboot v%s starting", LOXBOOT_VERSION_STRING);

    /* Find partitions by label */
    const esp_partition_t *p_sp  = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "loxstate");
    const esp_partition_t *p_sb  = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "loxbkup");
    const esp_partition_t *p_sa  = esp_partition_find_first(ESP_PARTITION_TYPE_APP,  ESP_PARTITION_SUBTYPE_ANY, "slot_a");
    const esp_partition_t *p_sbb = esp_partition_find_first(ESP_PARTITION_TYPE_APP,  ESP_PARTITION_SUBTYPE_ANY, "slot_b");

    if (!p_sp || !p_sb || !p_sa || !p_sbb) {
        ESP_LOGE(TAG, "Missing partition(s) — check partitions.csv");
        on_fatal(NULL, LOXBOOT_ERR_INVALID_STATE);
    }

    /* Init per-partition flash adapters */
    loxboot_esp32_flash_adapter_init(&g_mflash.state_primary, &g_mflash.ctx_sp);
    g_mflash.ctx_sp.partition = p_sp;
    loxboot_esp32_flash_adapter_init(&g_mflash.state_backup,  &g_mflash.ctx_sb);
    g_mflash.ctx_sb.partition = p_sb;
    loxboot_esp32_flash_adapter_init(&g_mflash.slot_a,        &g_mflash.ctx_sa);
    g_mflash.ctx_sa.partition = p_sa;
    loxboot_esp32_flash_adapter_init(&g_mflash.slot_b,        &g_mflash.ctx_sbb);
    g_mflash.ctx_sbb.partition = p_sbb;

    /* Store base addresses and sizes for routing */
    g_mflash.sp_base  = p_sp->address;   g_mflash.sp_size  = p_sp->size;
    g_mflash.sb_base  = p_sb->address;   g_mflash.sb_size  = p_sb->size;
    g_mflash.sa_base  = p_sa->address;   g_mflash.sa_size  = p_sa->size;
    g_mflash.sbb_base = p_sbb->address;  g_mflash.sbb_size = p_sbb->size;

    /* Build unified flash adapter */
    memset(&g_loxboot, 0, sizeof(g_loxboot));
    g_loxboot.flash.ctx   = &g_mflash;
    g_loxboot.flash.read  = mflash_read;
    g_loxboot.flash.write = mflash_write;
    g_loxboot.flash.erase = mflash_erase;

    g_loxboot.transport.read_byte  = usb_read_byte;
    g_loxboot.transport.write_byte = usb_write_byte;
    g_loxboot.transport.flush      = lox_uart_flush;

    g_loxboot.clock.now_ms = clock_now_ms;

    g_loxboot.hal.on_fatal = on_fatal;

    g_loxboot.platform.boot_state_primary_base = p_sp->address;
    g_loxboot.platform.boot_state_backup_base  = p_sb->address;
    g_loxboot.platform.slot_a_base             = p_sa->address;
    g_loxboot.platform.slot_b_base             = p_sbb->address;
    g_loxboot.platform.slot_size               = p_sa->size;

    loxboot_err_t err = loxboot_init(&g_loxboot);
    if (err != LOXBOOT_OK) {
        ESP_LOGE(TAG, "loxboot_init failed: %d", (int)err);
        on_fatal(NULL, err);
    }

    ESP_LOGI(TAG, "Listening for update on UART0 (%dms window)...", LOXBOOT_UART_LISTEN_MS);
    loxboot_run(&g_loxboot);

    /* loxboot_run never returns on success */
    on_fatal(NULL, LOXBOOT_ERR_INVALID_STATE);
}
