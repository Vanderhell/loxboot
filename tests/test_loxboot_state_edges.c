#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "test_support.h"

/* Internal functions (not part of public API) exposed from src/loxboot_state.c. */
loxboot_err_t loxboot_state_read(loxboot_t *ctx, loxboot_state_t *out_state);
loxboot_err_t loxboot_state_write(loxboot_t *ctx, const loxboot_state_t *state);

static uint32_t slot_record_crc32(const loxboot_slot_record_t *rec)
{
    return loxboot_crc32((const uint8_t *)rec, offsetof(loxboot_slot_record_t, record_crc32));
}

static uint32_t state_crc32(const loxboot_state_t *st)
{
    return loxboot_crc32((const uint8_t *)st, offsetof(loxboot_state_t, state_crc32));
}

static void write_copy(test_flash_t *flash, uint32_t base, const loxboot_state_t *st)
{
    CHECK_EQ_INT(test_flash_erase(flash, base, sizeof(*st)), LOXBOOT_OK);
    CHECK_EQ_INT(test_flash_write(flash, base, (const uint8_t *)st, sizeof(*st)), LOXBOOT_OK);
}

static void test_dual_copy_primary_corrupt_backup_valid_restores_primary(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t good;
    loxboot_state_t bad;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&good, LOXBOOT_SLOT_A);
    bad = good;
    bad.state_crc32 ^= 0x01020304u;

    write_copy(&flash, ctx.platform.boot_state_primary_base, &bad);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &good);

    loxboot_state_t out;
    CHECK_EQ_INT(loxboot_state_read(&ctx, &out), LOXBOOT_OK);
    CHECK_EQ_INT(memcmp(&out, &good, sizeof(out)), 0);

    loxboot_state_t primary_after;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &primary_after);
    CHECK_EQ_INT(memcmp(&primary_after, &good, sizeof(primary_after)), 0);
}

static void test_dual_copy_restore_primary_erase_fail_propagates(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t good;
    loxboot_state_t bad;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&good, LOXBOOT_SLOT_A);
    bad = good;
    bad.state_crc32 ^= 0xDEADBEEFu;

    write_copy(&flash, ctx.platform.boot_state_primary_base, &bad);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &good);

    flash.fail_next_erase = true;
    loxboot_state_t out;
    CHECK_EQ_INT(loxboot_state_read(&ctx, &out), LOXBOOT_ERR_FLASH_ERASE);
}

static void test_dual_copy_restore_primary_write_fail_propagates(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t good;
    loxboot_state_t bad;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&good, LOXBOOT_SLOT_A);
    bad = good;
    bad.state_crc32 ^= 0x00000001u;

    write_copy(&flash, ctx.platform.boot_state_primary_base, &bad);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &good);

    flash.fail_next_write = true;
    loxboot_state_t out;
    CHECK_EQ_INT(loxboot_state_read(&ctx, &out), LOXBOOT_ERR_FLASH_WRITE);
}

static void test_dual_copy_both_corrupt_returns_record_corrupt(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.state_crc32 ^= 0x11111111u;
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);

    loxboot_state_t out;
    CHECK_EQ_INT(loxboot_state_read(&ctx, &out), LOXBOOT_ERR_RECORD_CORRUPT);
}

static void test_state_validation_magic_mismatch_rejected(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.magic = 0u;
    st.state_crc32 = state_crc32(&st);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);

    loxboot_state_t out;
    CHECK_EQ_INT(loxboot_state_read(&ctx, &out), LOXBOOT_ERR_RECORD_CORRUPT);
}

static void test_state_validation_crc_mismatch_rejected(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.slots[0].firmware_size = 123u;
    /* Do not update state_crc32 => mismatch. */
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);

    loxboot_state_t out;
    CHECK_EQ_INT(loxboot_state_read(&ctx, &out), LOXBOOT_ERR_RECORD_CORRUPT);
}

static void test_state_validation_active_slot_values(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;
    loxboot_state_t out;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);
    CHECK_EQ_INT(loxboot_state_read(&ctx, &out), LOXBOOT_OK);
    CHECK_EQ_INT(out.active_slot, (int)LOXBOOT_SLOT_A);

    test_build_default_state(&st, LOXBOOT_SLOT_B);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);
    CHECK_EQ_INT(loxboot_state_read(&ctx, &out), LOXBOOT_OK);
    CHECK_EQ_INT(out.active_slot, (int)LOXBOOT_SLOT_B);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.active_slot = 0xFFu;
    st.state_crc32 = state_crc32(&st);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);
    CHECK_EQ_INT(loxboot_state_read(&ctx, &out), LOXBOOT_ERR_RECORD_CORRUPT);
}

static void test_slot_record_validation_rejections(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    /* magic mismatch */
    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.slots[0].magic = 0u;
    st.slots[0].record_crc32 = slot_record_crc32(&st.slots[0]);
    st.state_crc32 = state_crc32(&st);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);
    CHECK_EQ_INT(loxboot_state_read(&ctx, &st), LOXBOOT_ERR_RECORD_CORRUPT);

    /* slot_id mismatch */
    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.slots[0].slot_id = (uint8_t)LOXBOOT_SLOT_B;
    st.slots[0].record_crc32 = slot_record_crc32(&st.slots[0]);
    st.state_crc32 = state_crc32(&st);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);
    CHECK_EQ_INT(loxboot_state_read(&ctx, &st), LOXBOOT_ERR_RECORD_CORRUPT);

    /* state outside enum */
    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.slots[0].state = 0xFFu;
    st.slots[0].record_crc32 = slot_record_crc32(&st.slots[0]);
    st.state_crc32 = state_crc32(&st);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);
    CHECK_EQ_INT(loxboot_state_read(&ctx, &st), LOXBOOT_ERR_RECORD_CORRUPT);

    /* flags != 0 */
    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.slots[0].flags = 1u;
    st.slots[0].record_crc32 = slot_record_crc32(&st.slots[0]);
    st.state_crc32 = state_crc32(&st);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);
    CHECK_EQ_INT(loxboot_state_read(&ctx, &st), LOXBOOT_ERR_RECORD_CORRUPT);

    /* record_crc32 mismatch */
    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.slots[0].record_crc32 ^= 0x01020304u;
    st.state_crc32 = state_crc32(&st);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);
    CHECK_EQ_INT(loxboot_state_read(&ctx, &st), LOXBOOT_ERR_RECORD_CORRUPT);
}

static void test_state_write_failure_propagation_and_partial_write_behavior(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);

    /* primary erase fail */
    flash.fail_erase_on_addr = true;
    flash.fail_addr = ctx.platform.boot_state_primary_base;
    CHECK_EQ_INT(loxboot_state_write(&ctx, &st), LOXBOOT_ERR_FLASH_ERASE);

    /* primary write fail */
    flash.fail_write_on_addr = true;
    flash.fail_addr = ctx.platform.boot_state_primary_base;
    CHECK_EQ_INT(loxboot_state_write(&ctx, &st), LOXBOOT_ERR_FLASH_WRITE);

    /* backup erase fail: primary copy will have been updated, backup remains old/erased */
    test_flash_reset(&flash);
    test_build_default_state(&st, LOXBOOT_SLOT_A);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);

    loxboot_state_t new_state = st;
    new_state.slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    new_state.slots[1].record_crc32 = slot_record_crc32(&new_state.slots[1]);
    new_state.state_crc32 = state_crc32(&new_state);

    flash.fail_erase_on_addr = true;
    flash.fail_addr = ctx.platform.boot_state_backup_base;
    CHECK_EQ_INT(loxboot_state_write(&ctx, &new_state), LOXBOOT_ERR_FLASH_ERASE);

    loxboot_state_t primary_after, backup_after;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &primary_after);
    test_read_state_copy(&flash, ctx.platform.boot_state_backup_base, &backup_after);

    /* primary was written first and should match new_state; backup remains old state */
    CHECK_EQ_INT(memcmp(&primary_after, &new_state, sizeof(primary_after)), 0);
    CHECK_EQ_INT(memcmp(&backup_after, &st, sizeof(backup_after)), 0);

    /* backup write fail: primary updated, backup erased (0xFF) */
    test_flash_reset(&flash);
    write_copy(&flash, ctx.platform.boot_state_primary_base, &st);
    write_copy(&flash, ctx.platform.boot_state_backup_base, &st);

    flash.fail_write_on_addr = true;
    flash.fail_addr = ctx.platform.boot_state_backup_base;
    CHECK_EQ_INT(loxboot_state_write(&ctx, &new_state), LOXBOOT_ERR_FLASH_WRITE);

    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &primary_after);
    test_read_state_copy(&flash, ctx.platform.boot_state_backup_base, &backup_after);
    CHECK_EQ_INT(memcmp(&primary_after, &new_state, sizeof(primary_after)), 0);
    for (size_t i = 0; i < sizeof(backup_after); i++) {
        CHECK_EQ_INT(((const uint8_t *)&backup_after)[i], 0xFF);
    }
}

int main(void)
{
    run_test("state_edges/dual_copy_primary_corrupt_backup_valid_restores_primary",
             test_dual_copy_primary_corrupt_backup_valid_restores_primary);
    run_test("state_edges/dual_copy_restore_primary_erase_fail_propagates",
             test_dual_copy_restore_primary_erase_fail_propagates);
    run_test("state_edges/dual_copy_restore_primary_write_fail_propagates",
             test_dual_copy_restore_primary_write_fail_propagates);
    run_test("state_edges/dual_copy_both_corrupt_returns_record_corrupt",
             test_dual_copy_both_corrupt_returns_record_corrupt);
    run_test("state_edges/state_validation_magic_mismatch_rejected",
             test_state_validation_magic_mismatch_rejected);
    run_test("state_edges/state_validation_crc_mismatch_rejected",
             test_state_validation_crc_mismatch_rejected);
    run_test("state_edges/state_validation_active_slot_values",
             test_state_validation_active_slot_values);
    run_test("state_edges/slot_record_validation_rejections",
             test_slot_record_validation_rejections);
    run_test("state_edges/state_write_failure_propagation_and_partial_write_behavior",
             test_state_write_failure_propagation_and_partial_write_behavior);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
