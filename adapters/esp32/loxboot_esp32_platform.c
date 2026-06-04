#include <stddef.h>
#include "loxboot/loxboot.h"
#include "loxboot_esp32_platform.h"

#if defined(ESP_PLATFORM) && !defined(LOXBOOT_ESP32_STUB_BUILD)
#include "esp_ota_ops.h"
#include "esp_system.h"
#endif

/* -------------------------------------------------------------------------
 * Internal helper
 * ---------------------------------------------------------------------- */

static const esp_partition_t *partition_for_slot(
    const loxboot_esp32_platform_ctx_t *ctx,
    loxboot_slot_id_t slot)
{
    if (ctx == NULL) {
        return NULL;
    }
    if (slot == LOXBOOT_SLOT_A) {
        return ctx->slot_a;
    }
    if (slot == LOXBOOT_SLOT_B) {
        return ctx->slot_b;
    }
    return NULL;
}

/* -------------------------------------------------------------------------
 * Init
 * ---------------------------------------------------------------------- */

void loxboot_esp32_platform_init(
    loxboot_t                    *out,
    loxboot_esp32_platform_ctx_t *platform,
    const esp_partition_t        *slot_a,
    const esp_partition_t        *slot_b)
{
    if (out == NULL || platform == NULL) {
        return;
    }

    platform->slot_a = slot_a;
    platform->slot_b = slot_b;

    out->platform_ops.ctx     = platform;
    out->platform_ops.handoff = loxboot_esp32_handoff;
}

/* -------------------------------------------------------------------------
 * Handoff
 * ---------------------------------------------------------------------- */

loxboot_err_t loxboot_esp32_handoff(void *ctx, loxboot_slot_id_t slot)
{
    loxboot_esp32_platform_ctx_t *esp_ctx = (loxboot_esp32_platform_ctx_t *)ctx;
    const esp_partition_t *partition = partition_for_slot(esp_ctx, slot);

    if (partition == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

#ifdef ESP_PLATFORM
    esp_err_t err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK) {
        return LOXBOOT_ERR_INVALID_STATE;
    }
    esp_restart();
    /* esp_restart() does not return */
#endif

    /* Stub path (unit tests without ESP_PLATFORM): return OK, caller validates */
    return LOXBOOT_OK;
}

/* -------------------------------------------------------------------------
 * Confirm running app
 * ---------------------------------------------------------------------- */

void loxboot_esp32_confirm_running_app(void)
{
#ifdef ESP_PLATFORM
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        return;
    }

    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
    }
    /* All other states (VALID, INVALID, NEW, UNDEFINED): do nothing */
#endif
}

/* -------------------------------------------------------------------------
 * OTA state sync
 * ---------------------------------------------------------------------- */

#ifdef ESP_PLATFORM
static loxboot_slot_state_t map_ota_state(esp_ota_img_states_t ota_state)
{
    switch (ota_state) {
        case ESP_OTA_IMG_NEW:
            return LOXBOOT_SLOT_STATE_EMPTY;
        case ESP_OTA_IMG_PENDING_VERIFY:
            return LOXBOOT_SLOT_STATE_PENDING;
        case ESP_OTA_IMG_VALID:
            return LOXBOOT_SLOT_STATE_VALID;
        case ESP_OTA_IMG_INVALID:
        case ESP_OTA_IMG_ABORTED:
            return LOXBOOT_SLOT_STATE_INVALID;
        default:
            return LOXBOOT_SLOT_STATE_EMPTY;
    }
}
#endif

loxboot_err_t loxboot_esp32_sync_state_from_ota(
    loxboot_t                          *ctx,
    const loxboot_esp32_platform_ctx_t *platform)
{
    if (ctx == NULL || platform == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

#ifdef ESP_PLATFORM
    /* Map each slot partition's OTA state into loxboot slot state */
    const esp_partition_t *partitions[2] = {
        platform->slot_a,
        platform->slot_b
    };

    for (int i = 0; i < 2; i++) {
        if (partitions[i] == NULL) {
            continue;
        }

        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(partitions[i], &ota_state) != ESP_OK) {
            continue;
        }

        loxboot_slot_state_t new_state = map_ota_state(ota_state);
        if ((uint8_t)new_state != ctx->state.slots[i].state) {
            ctx->state.slots[i].state = (uint8_t)new_state;
        }
    }

    /* Mark the running partition's slot as ACTIVE */
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running != NULL) {
        if (running == platform->slot_a) {
            ctx->state.active_slot = (uint8_t)LOXBOOT_SLOT_A;
        } else if (running == platform->slot_b) {
            ctx->state.active_slot = (uint8_t)LOXBOOT_SLOT_B;
        }
    }
#else
    (void)ctx;
    (void)platform;
#endif

    return LOXBOOT_OK;
}
