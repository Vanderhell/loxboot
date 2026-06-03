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

/* Counter incremented each boot without confirmation */
static void test_crash_loop_counter_increments(void)
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

/* Counter reaches threshold after N boots without confirm */
static void test_crash_loop_threshold_reached(void)
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
    write_valid_firmware(&flash, ctx.platform.slot_b_base, 256u);

    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].boot_attempts = (uint8_t)LOXBOOT_MAX_BOOT_ATTEMPTS;
    state.slots[0].firmware_size = 256u;
    state.slots[0].firmware_crc32 = loxboot_crc32(&flash.mem[ctx.platform.slot_a_base], 256u);
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));

    state.slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
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

    loxboot_state_t read_state;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &read_state);
    CHECK_EQ_INT(read_state.slots[0].state, LOXBOOT_SLOT_STATE_ROLLBACK);
}

/* loxboot_confirm_boot() resets counter */
static void test_confirm_boot_resets_counter(void)
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

    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_ACTIVE;
    state.slots[0].boot_attempts = 2u;
    state.slots[0].firmware_size = 256u;
    state.slots[0].firmware_crc32 = loxboot_crc32(&flash.mem[ctx.platform.slot_a_base], 256u);
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));

    test_seed_state(&flash, &ctx.platform, &state);

    loxboot_err_t err = loxboot_confirm_boot(&ctx);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    loxboot_state_t read_state;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &read_state);
    CHECK_EQ_INT(read_state.slots[0].boot_attempts, 0u);
    CHECK_EQ_INT(read_state.slots[0].state, LOXBOOT_SLOT_STATE_VALID);
}

/* Counter persists across reboots (simulated) */
static void test_crash_loop_counter_persists(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t state;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    /* First boot */
    test_build_default_state(&state, LOXBOOT_SLOT_A);

    write_valid_firmware(&flash, ctx.platform.slot_a_base, 256u);

    state.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    state.slots[0].boot_attempts = 0u;
    state.slots[0].firmware_size = 256u;
    state.slots[0].firmware_crc32 = loxboot_crc32(&flash.mem[ctx.platform.slot_a_base], 256u);
    state.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&state.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    state.state_crc32 = loxboot_crc32((const uint8_t *)&state, offsetof(loxboot_state_t, state_crc32));

    test_seed_state(&flash, &ctx.platform, &state);

    g_jump_count = 0;
    loxboot_set_jump_hook(test_jump_hook, NULL);

    loxboot_run(&ctx);

    CHECK_EQ_INT(g_jump_count, 1);

    /* Simulated second boot (counter already incremented to 1) */
    loxboot_state_t read_state;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &read_state);
    CHECK_EQ_INT(read_state.slots[0].boot_attempts, 1u);

    memset(&ctx, 0, sizeof(ctx));
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    g_jump_count = 0;
    loxboot_run(&ctx);

    CHECK_EQ_INT(g_jump_count, 1);

    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &read_state);
    CHECK_EQ_INT(read_state.slots[0].boot_attempts, 2u);
}

int main(void)
{
    run_test("crash_loop_counter_increments", test_crash_loop_counter_increments);
    run_test("crash_loop_threshold_reached", test_crash_loop_threshold_reached);
    run_test("confirm_boot_resets_counter", test_confirm_boot_resets_counter);
    run_test("crash_loop_counter_persists", test_crash_loop_counter_persists);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
