#ifndef LOXBOOT_ESP32_PLATFORM_H
#define LOXBOOT_ESP32_PLATFORM_H

/**
 * loxboot ESP32 platform layer.
 *
 * Provides handoff, OTA state sync, and confirm helpers for ESP32/ESP32-S3.
 *
 * On ESP32, loxboot never jumps directly to application code.
 * Instead it uses ESP-IDF OTA APIs:
 *   - esp_ota_set_boot_partition() marks the target partition for next boot
 *   - esp_restart() triggers the Espressif second-stage bootloader
 *   - The IDF bootloader loads, validates, and executes the image
 *
 * Required partition table (partitions.csv):
 *   otadata,  data, ota,   <addr>, 0x2000,    <- OTA data (boot selection)
 *   ota_0,    app,  ota_0, <addr>, <size>,     <- slot A
 *   ota_1,    app,  ota_1, <addr>, <size>,     <- slot B
 *
 * Required sdkconfig:
 *   CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y    (for automatic rollback)
 */

#include "loxboot/loxboot.h"

#if defined(ESP_PLATFORM) && !defined(LOXBOOT_ESP32_STUB_BUILD)
/* Real ESP-IDF build */
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#else
/* Host build or stub-based unit test */
typedef struct esp_partition_t esp_partition_t;
typedef int esp_err_t;
#define ESP_OK   0
#define ESP_FAIL 1
typedef enum {
    ESP_OTA_IMG_NEW            = 0,
    ESP_OTA_IMG_PENDING_VERIFY = 1,
    ESP_OTA_IMG_VALID          = 2,
    ESP_OTA_IMG_INVALID        = 3,
    ESP_OTA_IMG_ABORTED        = 4,
    ESP_OTA_IMG_UNDEFINED      = 0xFF,
} esp_ota_img_states_t;

/* Declarations resolved at link time by stub implementations in test file */
esp_err_t              esp_ota_set_boot_partition(const esp_partition_t *partition);
void                   esp_restart(void);
const esp_partition_t *esp_ota_get_running_partition(void);
esp_err_t              esp_ota_get_state_partition(const esp_partition_t *partition,
                                                   esp_ota_img_states_t *out_state);
esp_err_t              esp_ota_mark_app_valid_cancel_rollback(void);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * ESP32 platform context
 * ====================================================================== */

/**
 * loxboot_esp32_platform_ctx_t — Context for ESP32 platform_ops.
 *
 * Holds partition handles for slot A (ota_0) and slot B (ota_1).
 * Initialize via loxboot_esp32_platform_init().
 */
typedef struct {
    const esp_partition_t *slot_a;   /**< ota_0 partition */
    const esp_partition_t *slot_b;   /**< ota_1 partition */
} loxboot_esp32_platform_ctx_t;

/* =========================================================================
 * Init / slot mapping
 * ====================================================================== */

/**
 * loxboot_esp32_platform_init — Populate platform_ops for ESP32.
 *
 * Fills out->platform_ops.handoff and out->platform_ops.ctx so that
 * loxboot_run() uses esp_ota_set_boot_partition()+esp_restart() instead of
 * the ARM vector-table jump.
 *
 * Parameters:
 *   out      — loxboot context to configure
 *   platform — caller-provided platform context (must outlive loxboot_run)
 *   slot_a   — esp_partition handle for ota_0 (LOXBOOT_SLOT_A)
 *   slot_b   — esp_partition handle for ota_1 (LOXBOOT_SLOT_B)
 */
void loxboot_esp32_platform_init(
    loxboot_t                    *out,
    loxboot_esp32_platform_ctx_t *platform,
    const esp_partition_t        *slot_a,
    const esp_partition_t        *slot_b);

/* =========================================================================
 * Handoff (called by loxboot_run via platform_ops.handoff)
 * ====================================================================== */

/**
 * loxboot_esp32_handoff — Set OTA boot partition and restart.
 *
 * Maps LOXBOOT_SLOT_A -> ota_0, LOXBOOT_SLOT_B -> ota_1.
 * Calls esp_ota_set_boot_partition() then esp_restart().
 * Does NOT return on success.
 *
 * Returns LOXBOOT_ERR_INVALID_ARG if partition not found.
 * Returns LOXBOOT_ERR_INVALID_STATE if esp_ota_set_boot_partition fails.
 */
loxboot_err_t loxboot_esp32_handoff(void *ctx, loxboot_slot_id_t slot);

/* =========================================================================
 * Application-side confirm (call after self-test passes)
 * ====================================================================== */

/**
 * loxboot_esp32_confirm_running_app — Mark running OTA image as valid.
 *
 * Call this AFTER application self-test passes (NVS, watchdog, comms).
 * Do NOT call at the very start of app_main() — if the app crashes before
 * calling this, the IDF bootloader will automatically rollback on next boot
 * (requires CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y).
 *
 * If the running partition is in ESP_OTA_IMG_PENDING_VERIFY state:
 *   calls esp_ota_mark_app_valid_cancel_rollback()
 * Otherwise: does nothing (already valid or unknown state).
 */
void loxboot_esp32_confirm_running_app(void);

/* =========================================================================
 * OTA state synchronization
 * ====================================================================== */

/**
 * loxboot_esp32_sync_state_from_ota — Align loxboot slot state with IDF OTA.
 *
 * ESP-IDF OTA data partition is the source of truth for which partition boots.
 * Call this at startup to reconcile loxboot slot state with actual IDF state.
 *
 * Mapping:
 *   ESP_OTA_IMG_VALID            -> LOXBOOT_SLOT_STATE_VALID
 *   ESP_OTA_IMG_PENDING_VERIFY   -> LOXBOOT_SLOT_STATE_PENDING
 *   ESP_OTA_IMG_INVALID/ABORTED  -> LOXBOOT_SLOT_STATE_INVALID
 *   ESP_OTA_IMG_NEW / unknown    -> LOXBOOT_SLOT_STATE_EMPTY
 *
 * Returns LOXBOOT_OK on success, or LOXBOOT_ERR_FLASH_WRITE if state write fails.
 */
loxboot_err_t loxboot_esp32_sync_state_from_ota(
    loxboot_t                          *ctx,
    const loxboot_esp32_platform_ctx_t *platform);

#ifdef __cplusplus
}
#endif

#endif /* LOXBOOT_ESP32_PLATFORM_H */
