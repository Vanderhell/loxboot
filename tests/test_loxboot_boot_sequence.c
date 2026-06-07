#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "test_support.h"

static int g_jump_count = 0;
static uint32_t g_jump_slot_base = 0u;

static void test_jump_hook(void *ctx, uint32_t slot_base)
{
    (void)ctx;
    g_jump_count++;
    g_jump_slot_base = slot_base;
}

static void write_valid_firmware(test_flash_t *flash, uint32_t slot_base, uint32_t size)
{
    uint8_t fw[256];
    memset(fw, 0xAA, sizeof(fw));
    uint32_t remaining = size;
    uint32_t offset = 0u;
    while (remaining > 0u) {
        uint32_t chunk = (remaining > sizeof(fw)) ? sizeof(fw) : remaining;
        (void)test_flash_write(flash, slot_base + offset, fw, chunk);
        offset += chunk;
        remaining -= chunk;
    }
}

/* Normal boot from VALID slot */
static void test_normal_boot_valid_slot(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    test_build_default_state(&state, LOXBOOT_SLOT_A);

    write_valid_firmware(&flash, ctx.platform.slot_a_base, 256u);

    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].firmware_size = 256u;
    state.slots[0].firmware_crc32 = loxboot_crc32(&flash.mem[ctx.platform.slot_a_base], 256u);
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));

    test_seed_state(&flash, &ctx.platform, &state);

    g_jump_count = 0;
    loxboot_set_jump_hook(test_jump_hook, NULL);

    loxboot_run(&ctx);

    CHECK_EQ_INT(g_jump_count, 1);
    CHECK_EQ_U32(g_jump_slot_base, ctx.platform.slot_a_base);
    CHECK_EQ_INT(ctx.boot_reason, LOXBOOT_REASON_NORMAL);
}

/* PENDING slot verified and promoted to ACTIVE */
static void test_boot_pending_verified(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    test_build_default_state(&state, LOXBOOT_SLOT_B);

    write_valid_firmware(&flash, ctx.platform.slot_b_base, 256u);

    state.slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_PENDING;
    state.slots[1].firmware_size = 256u;
    state.slots[1].firmware_crc32 = loxboot_crc32(&flash.mem[ctx.platform.slot_b_base], 256u);
    state.slots[1].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[1], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));

    test_seed_state(&flash, &ctx.platform, &state);

    g_jump_count = 0;
    loxboot_set_jump_hook(test_jump_hook, NULL);

    loxboot_run(&ctx);

    CHECK_EQ_INT(g_jump_count, 1);
    CHECK_EQ_U32(g_jump_slot_base, ctx.platform.slot_b_base);
    CHECK_EQ_INT(ctx.boot_reason, LOXBOOT_REASON_UPDATE);

    loxboot_state_t read_state;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &read_state);
    CHECK_EQ_INT(read_state.slots[1].state, LOXBOOT_SLOT_STATE_ACTIVE);
}

/* PENDING slot CRC fail falls back to VALID slot */
static void test_boot_pending_crc_fail_fallback(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    test_build_default_state(&state, LOXBOOT_SLOT_B);

    write_valid_firmware(&flash, ctx.platform.slot_a_base, 256u);

    state.slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_PENDING;
    state.slots[1].firmware_size = 256u;
    state.slots[1].firmware_crc32 = 0xDEADBEEFu;
    state.slots[1].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[1], offsetof(loxboot_slot_record_t, record_crc32));

    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].firmware_size = 256u;
    state.slots[0].firmware_crc32 = loxboot_crc32(&flash.mem[ctx.platform.slot_a_base], 256u);
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));

    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));

    test_seed_state(&flash, &ctx.platform, &state);

    g_jump_count = 0;
    loxboot_set_jump_hook(test_jump_hook, NULL);

    loxboot_run(&ctx);

    CHECK_EQ_INT(g_jump_count, 1);
    CHECK_EQ_U32(g_jump_slot_base, ctx.platform.slot_a_base);
}

/* No valid slot at all -> on_fatal */
static void test_boot_no_valid_slot(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    test_build_default_state(&state, LOXBOOT_SLOT_A);
    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_INVALID;
    state.slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_EMPTY;
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    state.slots[1].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[1], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));

    test_seed_state(&flash, &ctx.platform, &state);

    g_jump_count = 0;
    loxboot_set_jump_hook(test_jump_hook, NULL);

    loxboot_run(&ctx);

    CHECK_EQ_INT(fatal.count, 1);
    CHECK_EQ_INT(fatal.last_reason, LOXBOOT_ERR_NO_VALID_SLOT);
    CHECK_EQ_INT(g_jump_count, 0);
}

/* Non-ARM default handoff without an explicit platform callback fails safely */
static void test_boot_no_platform_handoff_fails_safely(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    test_build_default_state(&state, LOXBOOT_SLOT_A);

    write_valid_firmware(&flash, ctx.platform.slot_a_base, 256u);

    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].firmware_size = 256u;
    state.slots[0].firmware_crc32 = loxboot_crc32(&flash.mem[ctx.platform.slot_a_base], 256u);
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));

    test_seed_state(&flash, &ctx.platform, &state);

    g_jump_count = 0;
    loxboot_set_jump_hook(NULL, NULL);
    ctx.platform_ops.handoff = NULL;
    ctx.platform_ops.ctx = NULL;

    loxboot_run(&ctx);

    CHECK_EQ_INT(fatal.count, 1);
    CHECK_EQ_INT(fatal.last_reason, LOXBOOT_ERR_INVALID_STATE);
    CHECK_EQ_INT(g_jump_count, 0);
}

/* Boot state both copies corrupt -> auto-recover to blank state -> no valid slot -> on_fatal */
static void test_boot_state_corrupt(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    memset(&flash.mem[ctx.platform.boot_state_primary_base], 0, sizeof(loxboot_state_t));
    memset(&flash.mem[ctx.platform.boot_state_backup_base], 0, sizeof(loxboot_state_t));

    g_jump_count = 0;
    loxboot_set_jump_hook(test_jump_hook, NULL);

    loxboot_run(&ctx);

    /* loxboot_run() auto-recovers from corrupt state by writing blank state,
     * then fails with NO_VALID_SLOT because no firmware has been committed. */
    CHECK_EQ_INT(fatal.count, 1);
    CHECK_EQ_INT(fatal.last_reason, LOXBOOT_ERR_NO_VALID_SLOT);
    CHECK_EQ_INT(g_jump_count, 0);
}

/* Boot attempts counter incremented before jump */
static void test_boot_attempts_incremented(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    test_build_default_state(&state, LOXBOOT_SLOT_A);

    write_valid_firmware(&flash, ctx.platform.slot_a_base, 256u);

    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].boot_attempts = 1u;
    state.slots[0].firmware_size = 256u;
    state.slots[0].firmware_crc32 = loxboot_crc32(&flash.mem[ctx.platform.slot_a_base], 256u);
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));

    test_seed_state(&flash, &ctx.platform, &state);

    g_jump_count = 0;
    loxboot_set_jump_hook(test_jump_hook, NULL);

    loxboot_run(&ctx);

    CHECK_EQ_INT(g_jump_count, 1);

    loxboot_state_t read_state;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &read_state);
    CHECK_EQ_INT(read_state.slots[0].boot_attempts, 2u);
}

int main(void)
{
    run_test("normal_boot_valid_slot", test_normal_boot_valid_slot);
    run_test("boot_pending_verified", test_boot_pending_verified);
    run_test("boot_pending_crc_fail_fallback", test_boot_pending_crc_fail_fallback);
    run_test("boot_no_valid_slot", test_boot_no_valid_slot);
    run_test("boot_no_platform_handoff_fails_safely", test_boot_no_platform_handoff_fails_safely);
    run_test("boot_state_corrupt", test_boot_state_corrupt);
    run_test("boot_attempts_incremented", test_boot_attempts_incremented);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
