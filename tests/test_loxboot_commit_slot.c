#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "test_support.h"

static uint32_t slot_record_crc32(const loxboot_slot_record_t *rec)
{
    return loxboot_crc32((const uint8_t *)rec, offsetof(loxboot_slot_record_t, record_crc32));
}

static void test_commit_slot_a(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_B);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_A, 100u, 0xAABBCCDDu), LOXBOOT_OK);
    CHECK_EQ_INT(ctx.state.slots[0].state, (int)LOXBOOT_SLOT_STATE_PENDING);
    CHECK_EQ_U32(ctx.state.slots[0].firmware_size, 100u);
    CHECK_EQ_U32(ctx.state.slots[0].firmware_crc32, 0xAABBCCDDu);
    CHECK_EQ_U32(ctx.state.slots[0].record_crc32, slot_record_crc32(&ctx.state.slots[0]));

    loxboot_state_t p, b;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &p);
    test_read_state_copy(&flash, ctx.platform.boot_state_backup_base, &b);
    CHECK_EQ_INT(memcmp(&p, &b, sizeof(p)), 0);
    CHECK_EQ_INT(p.slots[0].state, (int)LOXBOOT_SLOT_STATE_PENDING);
}

static void test_commit_slot_b(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_B, 200u, 0x01020304u), LOXBOOT_OK);
    CHECK_EQ_INT(ctx.state.slots[1].state, (int)LOXBOOT_SLOT_STATE_PENDING);
}

static void test_rejects_null_or_uninitialized(void)
{
    CHECK_EQ_INT(loxboot_commit_slot(NULL, LOXBOOT_SLOT_A, 1u, 0u), LOXBOOT_ERR_INVALID_ARG);

    loxboot_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_A, 1u, 0u), LOXBOOT_ERR_INVALID_STATE);
}

static void test_rejects_invalid_slot(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    CHECK_EQ_INT(loxboot_commit_slot(&ctx, (loxboot_slot_id_t)9, 1u, 0u), LOXBOOT_ERR_INVALID_ARG);
}

static void test_rejects_size_bounds(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_B, 0u, 0u), LOXBOOT_ERR_INVALID_ARG);
    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_B, ctx.platform.slot_size + 1u, 0u), LOXBOOT_ERR_INVALID_ARG);
}

static void test_rejects_active_slot(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_A, 10u, 0u), LOXBOOT_ERR_INVALID_STATE);
}

static void test_propagates_flash_failures(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &st);

    flash.fail_next_read = true;
    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_B, 10u, 0u), LOXBOOT_ERR_FLASH_READ);

    test_seed_state(&flash, &ctx.platform, &st);
    flash.fail_next_erase = true;
    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_B, 10u, 0u), LOXBOOT_ERR_FLASH_ERASE);

    test_seed_state(&flash, &ctx.platform, &st);
    flash.fail_next_write = true;
    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_B, 10u, 0u), LOXBOOT_ERR_FLASH_WRITE);
}

int main(void)
{
    run_test("commit_slot/slot_a", test_commit_slot_a);
    run_test("commit_slot/slot_b", test_commit_slot_b);
    run_test("commit_slot/rejects_null_or_uninitialized", test_rejects_null_or_uninitialized);
    run_test("commit_slot/rejects_invalid_slot", test_rejects_invalid_slot);
    run_test("commit_slot/rejects_size_bounds", test_rejects_size_bounds);
    run_test("commit_slot/rejects_active_slot", test_rejects_active_slot);
    run_test("commit_slot/propagates_flash_failures", test_propagates_flash_failures);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}

