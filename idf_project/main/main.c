#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_ota_ops.h"

#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"
#include "loxboot_flash_esp32.h"
#include "loxboot_esp32_platform.h"

static const char *TAG = "loxboot";

/* Internal core helper (exported from loxboot_state.c, not in public header) */
extern loxboot_err_t loxboot_state_read(loxboot_t *ctx, loxboot_state_t *out_state);

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
static loxboot_t                    g_loxboot;
static loxboot_esp32_platform_ctx_t g_esp32_platform;

void app_main(void)
{
#if LOXBOOT_ESP32_AUTO_CONFIRM
    /* Confirm running app if in PENDING_VERIFY state (after OTA update).
     * Called before any other init so rollback triggers on any crash after this point. */
    loxboot_esp32_confirm_running_app();
#else
    ESP_LOGW(TAG, "AUTO_CONFIRM disabled - pending OTA images will remain pending for rollback tests");
#endif

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

    /* Wire ESP32 OTA handoff (esp_ota_set_boot_partition + esp_restart) */
    loxboot_esp32_platform_init(&g_loxboot, &g_esp32_platform, p_sa, p_sbb);

    loxboot_err_t err = loxboot_init(&g_loxboot);
    if (err != LOXBOOT_OK) {
        ESP_LOGE(TAG, "loxboot_init failed: %d", (int)err);
        on_fatal(NULL, err);
    }

    /* Ensure loxboot has a valid state on first boot (fresh flash) */
    loxboot_state_t st;
    if (loxboot_state_read(&g_loxboot, &st) != LOXBOOT_OK) {
        loxboot_format_state(&g_loxboot, LOXBOOT_SLOT_A);
    }

    /* Align loxboot state with IDF OTA truth (which partition is running) */
    loxboot_esp32_sync_state_from_ota(&g_loxboot, &g_esp32_platform);

    /* DIAGNOSTIC: report which partition is actually running */
    {
        const esp_partition_t *run = esp_ota_get_running_partition();
        const esp_partition_t *boot = esp_ota_get_boot_partition();
        ESP_LOGW(TAG, "RUNNING partition: %s @ 0x%06lx",
                 run ? run->label : "?", (unsigned long)(run ? run->address : 0));
        ESP_LOGW(TAG, "BOOT (next) partition: %s @ 0x%06lx",
                 boot ? boot->label : "?", (unsigned long)(boot ? boot->address : 0));
        ESP_LOGW(TAG, "loxboot active_slot=%d", (int)g_loxboot.state.active_slot);
    }

    /*
     * ESP32 model: loxboot is an OTA UPDATER, not a bootloader.
     * The IDF second-stage bootloader owns boot selection (via otadata).
     * We loop running UART update sessions:
     *   - on a committed update  -> handoff (set_boot_partition + restart)
     *   - on a bare REBOOT       -> esp_restart()
     *   - on timeout (no update) -> keep listening (the "application")
     */
    ESP_LOGI(TAG, "Listening for update via USB...");
    while (1) {
        loxboot_uart_session_t session;
        memset(&session, 0, sizeof(session));
        session.boot      = &g_loxboot;
        session.listen_ms = LOXBOOT_UART_LISTEN_MS;

        err = loxboot_uart_run_session(&session);
        (void)err;

        if (session._commit_done) {
            ESP_LOGI(TAG, "Update committed to slot %d — handing off via OTA",
                     (int)session._committed_slot);
            loxboot_err_t herr = loxboot_esp32_handoff(&g_esp32_platform, session._committed_slot);
            /* handoff calls esp_restart() on success — does not return */
            ESP_LOGE(TAG, "HANDOFF FAILED: err=%d (esp_ota_set_boot_partition rejected image)",
                     (int)herr);
        }

        if (session._reboot_requested) {
            ESP_LOGI(TAG, "Reboot requested (no update) — restarting");
            esp_restart();
        }

        /* Timeout with no update — yield to IDLE/watchdog, then keep listening.
         * usb_serial_jtag_read_bytes may return without blocking the full window,
         * so this delay prevents a busy-spin that starves the idle task. */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
