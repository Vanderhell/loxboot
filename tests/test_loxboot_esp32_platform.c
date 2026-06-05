/*
 * Unit tests for loxboot_esp32_platform — uses ESP-IDF stubs.
 * Tests run on host without real ESP-IDF.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* Include stubs BEFORE platform header so ESP_PLATFORM is defined */
#include "esp_ota_stub.h"
#include "loxboot/loxboot.h"
#include "loxboot_esp32_platform.h"
#include "test_support.h"

/* =========================================================================
 * ESP-IDF stubs — function implementations (struct defined in esp_ota_stub.h)
 * ====================================================================== */

static struct esp_partition_t s_ota0 = { .id = 0, .label = "ota_0", .address = 0x20000 };
static struct esp_partition_t s_ota1 = { .id = 1, .label = "ota_1", .address = 0x1A0000 };

/* Stub state tracking */
static const esp_partition_t *g_set_boot_called_with = NULL;
static bool                   g_set_boot_should_fail  = false;
static bool                   g_restart_called         = false;
static esp_ota_img_states_t   g_ota0_state = ESP_OTA_IMG_VALID;
static esp_ota_img_states_t   g_ota1_state = ESP_OTA_IMG_NEW;
static const esp_partition_t *g_running_partition = &s_ota0;
static bool                   g_mark_valid_called  = false;

/* Stub implementations */
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition)
{
    g_set_boot_called_with = partition;
    return g_set_boot_should_fail ? 1 /* ESP_FAIL */ : 0 /* ESP_OK */;
}

void esp_restart(void)
{
    g_restart_called = true;
}

const esp_partition_t *esp_ota_get_running_partition(void)
{
    return g_running_partition;
}

esp_err_t esp_ota_get_state_partition(const esp_partition_t *p, esp_ota_img_states_t *out)
{
    if (p == &s_ota0) { *out = g_ota0_state; return 0; }
    if (p == &s_ota1) { *out = g_ota1_state; return 0; }
    return 1;
}

esp_err_t esp_ota_mark_app_valid_cancel_rollback(void)
{
    g_mark_valid_called = true;
    return 0;
}

static void stub_reset(void)
{
    g_set_boot_called_with = NULL;
    g_set_boot_should_fail  = false;
    g_restart_called         = false;
    g_ota0_state = ESP_OTA_IMG_VALID;
    g_ota1_state = ESP_OTA_IMG_NEW;
    g_running_partition = &s_ota0;
    g_mark_valid_called  = false;
}

/* =========================================================================
 * Tests: handoff slot mapping
 * ====================================================================== */

static void test_handoff_slot_a_maps_to_ota0(void)
{
    stub_reset();

    loxboot_esp32_platform_ctx_t esp_ctx = { .slot_a = &s_ota0, .slot_b = &s_ota1 };
    loxboot_err_t err = loxboot_esp32_handoff(&esp_ctx, LOXBOOT_SLOT_A);

    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK(g_set_boot_called_with == &s_ota0);
    CHECK(g_restart_called == true);
}

static void test_handoff_slot_b_maps_to_ota1(void)
{
    stub_reset();

    loxboot_esp32_platform_ctx_t esp_ctx = { .slot_a = &s_ota0, .slot_b = &s_ota1 };
    loxboot_err_t err = loxboot_esp32_handoff(&esp_ctx, LOXBOOT_SLOT_B);

    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK(g_set_boot_called_with == &s_ota1);
    CHECK(g_restart_called == true);
}

static void test_handoff_null_ctx_returns_error(void)
{
    stub_reset();
    loxboot_err_t err = loxboot_esp32_handoff(NULL, LOXBOOT_SLOT_A);
    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_ARG);
    CHECK(g_set_boot_called_with == NULL);
    CHECK(g_restart_called == false);
}

static void test_handoff_set_boot_failure_returns_error(void)
{
    stub_reset();
    g_set_boot_should_fail = true;

    loxboot_esp32_platform_ctx_t esp_ctx = { .slot_a = &s_ota0, .slot_b = &s_ota1 };
    loxboot_err_t err = loxboot_esp32_handoff(&esp_ctx, LOXBOOT_SLOT_A);

    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_STATE);
    CHECK(g_restart_called == false);  /* restart must NOT be called on failure */
}

static void test_handoff_restart_not_called_on_failure(void)
{
    stub_reset();
    g_set_boot_should_fail = true;

    loxboot_esp32_platform_ctx_t esp_ctx = { .slot_a = &s_ota0, .slot_b = &s_ota1 };
    loxboot_esp32_handoff(&esp_ctx, LOXBOOT_SLOT_B);

    CHECK(g_restart_called == false);
}

/* =========================================================================
 * Tests: platform_init wires platform_ops
 * ====================================================================== */

static void test_platform_init_sets_handoff(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_esp32_platform_ctx_t esp_ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);

    loxboot_esp32_platform_init(&ctx, &esp_ctx, &s_ota0, &s_ota1);

    CHECK(ctx.platform_ops.handoff == loxboot_esp32_handoff);
    CHECK(ctx.platform_ops.ctx == &esp_ctx);
    CHECK(esp_ctx.slot_a == &s_ota0);
    CHECK(esp_ctx.slot_b == &s_ota1);
}

/* =========================================================================
 * Tests: confirm_running_app
 * ====================================================================== */

static void test_confirm_calls_mark_valid_when_pending(void)
{
    stub_reset();
    g_ota0_state = ESP_OTA_IMG_PENDING_VERIFY;
    g_running_partition = &s_ota0;

    loxboot_esp32_confirm_running_app();

    CHECK(g_mark_valid_called == true);
}

static void test_confirm_does_nothing_when_already_valid(void)
{
    stub_reset();
    g_ota0_state = ESP_OTA_IMG_VALID;
    g_running_partition = &s_ota0;

    loxboot_esp32_confirm_running_app();

    CHECK(g_mark_valid_called == false);
}

static void test_confirm_does_nothing_when_invalid(void)
{
    stub_reset();
    g_ota0_state = ESP_OTA_IMG_INVALID;
    g_running_partition = &s_ota0;

    loxboot_esp32_confirm_running_app();

    CHECK(g_mark_valid_called == false);
}

/* =========================================================================
 * Tests: sync_state_from_ota
 * ====================================================================== */

static void test_sync_maps_valid_to_valid(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    stub_reset();
    g_ota0_state = ESP_OTA_IMG_VALID;
    g_ota1_state = ESP_OTA_IMG_NEW;

    loxboot_esp32_platform_ctx_t esp_ctx = { .slot_a = &s_ota0, .slot_b = &s_ota1 };
    loxboot_err_t err = loxboot_esp32_sync_state_from_ota(&ctx, &esp_ctx);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(ctx.state.slots[LOXBOOT_SLOT_A].state, LOXBOOT_SLOT_STATE_VALID);
    CHECK_EQ_INT(ctx.state.slots[LOXBOOT_SLOT_B].state, LOXBOOT_SLOT_STATE_EMPTY);
}

static void test_sync_maps_pending_verify_to_pending(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    stub_reset();
    g_ota1_state = ESP_OTA_IMG_PENDING_VERIFY;

    loxboot_esp32_platform_ctx_t esp_ctx = { .slot_a = &s_ota0, .slot_b = &s_ota1 };
    loxboot_esp32_sync_state_from_ota(&ctx, &esp_ctx);
    CHECK_EQ_INT(ctx.state.slots[LOXBOOT_SLOT_B].state, LOXBOOT_SLOT_STATE_PENDING);
}

static void test_sync_maps_invalid_to_invalid(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    stub_reset();
    g_ota1_state = ESP_OTA_IMG_INVALID;

    loxboot_esp32_platform_ctx_t esp_ctx = { .slot_a = &s_ota0, .slot_b = &s_ota1 };
    loxboot_esp32_sync_state_from_ota(&ctx, &esp_ctx);
    CHECK_EQ_INT(ctx.state.slots[LOXBOOT_SLOT_B].state, LOXBOOT_SLOT_STATE_INVALID);
}

static void test_sync_sets_active_slot_from_running_partition(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    test_build_default_state(&state, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &state);
    loxboot_init(&ctx);

    stub_reset();
    g_running_partition = &s_ota1;  /* running from slot B */

    loxboot_esp32_platform_ctx_t esp_ctx = { .slot_a = &s_ota0, .slot_b = &s_ota1 };
    loxboot_esp32_sync_state_from_ota(&ctx, &esp_ctx);
    CHECK_EQ_INT(ctx.state.active_slot, (int)LOXBOOT_SLOT_B);
}

static void test_sync_null_ctx_returns_error(void)
{
    loxboot_esp32_platform_ctx_t esp_ctx = { .slot_a = &s_ota0, .slot_b = &s_ota1 };
    CHECK_EQ_INT(loxboot_esp32_sync_state_from_ota(NULL, &esp_ctx), LOXBOOT_ERR_INVALID_ARG);
    CHECK_EQ_INT(loxboot_esp32_sync_state_from_ota(NULL, NULL),     LOXBOOT_ERR_INVALID_ARG);
}

/* =========================================================================
 * Tests: platform_ops wired into loxboot_run (via test hook)
 * ====================================================================== */

static loxboot_slot_id_t g_handoff_received_slot = (loxboot_slot_id_t)0xFF;
static bool g_handoff_stub_called = false;

static loxboot_err_t stub_handoff(void *ctx, loxboot_slot_id_t slot)
{
    (void)ctx;
    g_handoff_stub_called = true;
    g_handoff_received_slot = slot;
    return LOXBOOT_OK;
}

static void test_loxboot_run_calls_platform_handoff(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    /* Build state with SLOT_A VALID — same pattern as test_loxboot_boot_sequence.c */
    test_build_default_state(&state, LOXBOOT_SLOT_A);

    static const uint8_t dummy_fw[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    test_flash_write(&flash, ctx.platform.slot_a_base, dummy_fw, sizeof(dummy_fw));

    state.slots[0].state         = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].firmware_size = sizeof(dummy_fw);
    state.slots[0].firmware_crc32 = loxboot_crc32(dummy_fw, sizeof(dummy_fw));
    state.slots[0].record_crc32   = loxboot_crc32((const uint8_t *)&state.slots[0],
                                        offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state,
                                      offsetof(loxboot_state_t, state_crc32));
    test_seed_state(&flash, &ctx.platform, &state);

    /* Wire stub handoff */
    g_handoff_stub_called    = false;
    g_handoff_received_slot  = (loxboot_slot_id_t)0xFF;
    ctx.platform_ops.handoff = stub_handoff;
    ctx.platform_ops.ctx     = NULL;

    loxboot_run(&ctx);

    CHECK(g_handoff_stub_called == true);
    CHECK_EQ_INT(g_handoff_received_slot, (int)LOXBOOT_SLOT_A);
}

/* =========================================================================
 * Main
 * ====================================================================== */

int main(void)
{
    run_test("esp32/handoff_slot_a_maps_to_ota0",          test_handoff_slot_a_maps_to_ota0);
    run_test("esp32/handoff_slot_b_maps_to_ota1",          test_handoff_slot_b_maps_to_ota1);
    run_test("esp32/handoff_null_ctx_returns_error",        test_handoff_null_ctx_returns_error);
    run_test("esp32/handoff_set_boot_failure_returns_error",test_handoff_set_boot_failure_returns_error);
    run_test("esp32/handoff_restart_not_called_on_failure", test_handoff_restart_not_called_on_failure);
    run_test("esp32/platform_init_sets_handoff",            test_platform_init_sets_handoff);
    run_test("esp32/confirm_calls_mark_valid_when_pending", test_confirm_calls_mark_valid_when_pending);
    run_test("esp32/confirm_does_nothing_when_valid",       test_confirm_does_nothing_when_already_valid);
    run_test("esp32/confirm_does_nothing_when_invalid",     test_confirm_does_nothing_when_invalid);
    run_test("esp32/sync_maps_valid",                       test_sync_maps_valid_to_valid);
    run_test("esp32/sync_maps_pending_verify",              test_sync_maps_pending_verify_to_pending);
    run_test("esp32/sync_maps_invalid",                     test_sync_maps_invalid_to_invalid);
    run_test("esp32/sync_sets_active_slot_from_running",    test_sync_sets_active_slot_from_running_partition);
    run_test("esp32/sync_null_ctx_returns_error",           test_sync_null_ctx_returns_error);
    run_test("esp32/loxboot_run_calls_platform_handoff",    test_loxboot_run_calls_platform_handoff);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
