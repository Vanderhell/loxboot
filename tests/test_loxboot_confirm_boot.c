#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "test_support.h"

static uint32_t state_crc32(const loxboot_state_t *st)
{
    return loxboot_crc32((const uint8_t *)st, offsetof(loxboot_state_t, state_crc32));
}

static uint32_t slot_record_crc32(const loxboot_slot_record_t *rec)
{
    return loxboot_crc32((const uint8_t *)rec, offsetof(loxboot_slot_record_t, record_crc32));
}

static void test_confirms_active_slot(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_ACTIVE;
    st.slots[0].boot_attempts = 2u;
    st.slots[0].record_crc32 = slot_record_crc32(&st.slots[0]);
    st.state_crc32 = state_crc32(&st);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_confirm_boot(&ctx), LOXBOOT_OK);
    CHECK_EQ_INT(ctx.state.slots[0].state, (int)LOXBOOT_SLOT_STATE_VALID);
    CHECK_EQ_INT(ctx.state.slots[0].boot_attempts, 0);

    loxboot_state_t p, b;
    test_read_state_copy(&flash, ctx.platform.boot_state_primary_base, &p);
    test_read_state_copy(&flash, ctx.platform.boot_state_backup_base, &b);
    CHECK_EQ_INT(memcmp(&p, &b, sizeof(p)), 0);
}

static void test_confirms_valid_slot_idempotently(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_B);
    st.slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    st.slots[1].boot_attempts = 1u;
    st.slots[1].record_crc32 = slot_record_crc32(&st.slots[1]);
    st.state_crc32 = state_crc32(&st);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_confirm_boot(&ctx), LOXBOOT_OK);
    CHECK_EQ_INT(ctx.state.slots[1].state, (int)LOXBOOT_SLOT_STATE_VALID);
    CHECK_EQ_INT(ctx.state.slots[1].boot_attempts, 0);
}

static void test_rejects_invalid_active_slot(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.active_slot = 0xFFu;
    st.state_crc32 = state_crc32(&st);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_confirm_boot(&ctx), LOXBOOT_ERR_RECORD_CORRUPT);
}

static void test_rejects_uninitialized_ctx(void)
{
    loxboot_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    CHECK_EQ_INT(loxboot_confirm_boot(&ctx), LOXBOOT_ERR_INVALID_STATE);
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
    st.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_ACTIVE;
    st.slots[0].record_crc32 = slot_record_crc32(&st.slots[0]);
    st.state_crc32 = state_crc32(&st);
    test_seed_state(&flash, &ctx.platform, &st);

    flash.fail_next_read = true;
    CHECK_EQ_INT(loxboot_confirm_boot(&ctx), LOXBOOT_ERR_FLASH_READ);

    test_seed_state(&flash, &ctx.platform, &st);
    flash.fail_next_erase = true;
    CHECK_EQ_INT(loxboot_confirm_boot(&ctx), LOXBOOT_ERR_FLASH_ERASE);

    test_seed_state(&flash, &ctx.platform, &st);
    flash.fail_next_write = true;
    CHECK_EQ_INT(loxboot_confirm_boot(&ctx), LOXBOOT_ERR_FLASH_WRITE);
}

int main(void)
{
    run_test("confirm_boot/confirms_active_slot", test_confirms_active_slot);
    run_test("confirm_boot/confirms_valid_slot_idempotently", test_confirms_valid_slot_idempotently);
    run_test("confirm_boot/rejects_invalid_active_slot", test_rejects_invalid_active_slot);
    run_test("confirm_boot/rejects_uninitialized_ctx", test_rejects_uninitialized_ctx);
    run_test("confirm_boot/propagates_flash_failures", test_propagates_flash_failures);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
