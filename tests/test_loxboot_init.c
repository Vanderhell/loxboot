#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "test_support.h"

static void test_valid_minimal(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);

    /* clock/transport are intentionally omitted in v0.2.0-core */
    ctx.clock.now_ms = NULL;
    ctx.transport.read_byte = NULL;
    ctx.transport.write_byte = NULL;
    ctx.transport.flush = NULL;

    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);
    CHECK(ctx.initialized);
    CHECK_EQ_INT(loxboot_get_boot_reason(&ctx), LOXBOOT_REASON_UNKNOWN);
}

static void test_null_ctx(void)
{
    CHECK_EQ_INT(loxboot_init(NULL), LOXBOOT_ERR_INVALID_ARG);
}

static void test_missing_flash_callbacks(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);

    ctx.flash.read = NULL;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);

    test_make_valid_ctx(&ctx, &flash, &fatal);
    ctx.flash.write = NULL;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);

    test_make_valid_ctx(&ctx, &flash, &fatal);
    ctx.flash.erase = NULL;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);
}

static void test_missing_fatal_handler(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    ctx.hal.on_fatal = NULL;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);
}

static void test_zero_slot_size(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    ctx.platform.slot_size = 0u;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);
}

static void test_duplicate_slot_bases(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    ctx.platform.slot_b_base = ctx.platform.slot_a_base;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);
}

static void test_duplicate_state_bases(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    ctx.platform.boot_state_backup_base = ctx.platform.boot_state_primary_base;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);
}

static void test_overlapping_slots(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);

    /* Make B start within A. */
    ctx.platform.slot_b_base = ctx.platform.slot_a_base + (ctx.platform.slot_size / 2u);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);
}

static void test_adjacent_slots_allowed(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    ctx.platform.slot_b_base = ctx.platform.slot_a_base + ctx.platform.slot_size;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);
}

static void test_overflowing_slot_range_rejected(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    ctx.platform.slot_a_base = 0xFFFFFFF0u;
    ctx.platform.slot_size = 0x20u;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);
}

static void test_overlapping_state_copies(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);

    ctx.platform.boot_state_backup_base = ctx.platform.boot_state_primary_base + (uint32_t)(sizeof(loxboot_state_t) / 2u);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);
}

static void test_state_overlaps_slot(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);

    ctx.platform.boot_state_primary_base = ctx.platform.slot_a_base;
    ctx.platform.boot_state_backup_base = ctx.platform.boot_state_primary_base + 0x200u;
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_ERR_INVALID_ARG);
}

/* =========================================================================
 * loxboot_format_state() unit tests
 * ====================================================================== */

static void test_format_state_null_ctx(void)
{
    CHECK_EQ_INT(loxboot_format_state(NULL, LOXBOOT_SLOT_A), LOXBOOT_ERR_INVALID_ARG);
}

static void test_format_state_uninitialized_ctx(void)
{
    loxboot_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    /* ctx.initialized is false — should reject */
    CHECK_EQ_INT(loxboot_format_state(&ctx, LOXBOOT_SLOT_A), LOXBOOT_ERR_INVALID_ARG);
}

static void test_format_state_valid_slot_a(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    loxboot_err_t err = loxboot_format_state(&ctx, LOXBOOT_SLOT_A);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    /* State must be readable after format */
    loxboot_state_t st;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    CHECK_EQ_INT(st.active_slot, (int)LOXBOOT_SLOT_A);
    CHECK_EQ_INT(st.slots[LOXBOOT_SLOT_A].state, LOXBOOT_SLOT_STATE_EMPTY);
    CHECK_EQ_INT(st.slots[LOXBOOT_SLOT_B].state, LOXBOOT_SLOT_STATE_EMPTY);
}

static void test_format_state_valid_slot_b(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    loxboot_err_t err = loxboot_format_state(&ctx, LOXBOOT_SLOT_B);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    loxboot_state_t st;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    CHECK_EQ_INT(st.active_slot, (int)LOXBOOT_SLOT_B);
}

static void test_format_state_writes_primary(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    loxboot_format_state(&ctx, LOXBOOT_SLOT_A);

    /* Primary copy must be a valid state */
    loxboot_state_t primary;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &primary);
    CHECK_EQ_U32(primary.magic, LOXBOOT_STATE_MAGIC);
    CHECK_EQ_INT(primary.slots[0].slot_id, (int)LOXBOOT_SLOT_A);
    CHECK_EQ_INT(primary.slots[1].slot_id, (int)LOXBOOT_SLOT_B);
}

static void test_format_state_writes_backup(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    loxboot_init(&ctx);

    loxboot_format_state(&ctx, LOXBOOT_SLOT_A);

    /* Backup copy must be identical to primary */
    loxboot_state_t primary, backup;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &primary);
    test_read_state_copy(&flash, ctx.platform.boot_state_backup_base,  &backup);
    CHECK_EQ_U32(primary.magic,       backup.magic);
    CHECK_EQ_U32(primary.state_crc32, backup.state_crc32);
    CHECK_EQ_INT(primary.active_slot, backup.active_slot);
}

int main(void)
{
    run_test("init/valid_minimal", test_valid_minimal);
    run_test("init/null_ctx", test_null_ctx);
    run_test("init/missing_flash_callbacks", test_missing_flash_callbacks);
    run_test("init/missing_fatal_handler", test_missing_fatal_handler);
    run_test("init/zero_slot_size", test_zero_slot_size);
    run_test("init/duplicate_slot_bases", test_duplicate_slot_bases);
    run_test("init/duplicate_state_bases", test_duplicate_state_bases);
    run_test("init/overlapping_slots", test_overlapping_slots);
    run_test("init/adjacent_slots_allowed", test_adjacent_slots_allowed);
    run_test("init/overflowing_slot_range_rejected", test_overflowing_slot_range_rejected);
    run_test("init/overlapping_state_copies", test_overlapping_state_copies);
    run_test("init/state_overlaps_slot", test_state_overlaps_slot);

    run_test("format_state/null_ctx",          test_format_state_null_ctx);
    run_test("format_state/uninitialized_ctx", test_format_state_uninitialized_ctx);
    run_test("format_state/valid_slot_a",      test_format_state_valid_slot_a);
    run_test("format_state/valid_slot_b",      test_format_state_valid_slot_b);
    run_test("format_state/writes_primary",    test_format_state_writes_primary);
    run_test("format_state/writes_backup",     test_format_state_writes_backup);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
