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

static void test_invalidate_slot_a(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_B);
    st.slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_PENDING;
    st.slots[0].firmware_size = 111u;
    st.slots[0].firmware_crc32 = 0xDEADBEEFu;
    st.slots[0].record_crc32 = loxboot_crc32((const uint8_t *)&st.slots[0], offsetof(loxboot_slot_record_t, record_crc32));
    st.state_crc32 = state_crc32(&st);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, LOXBOOT_SLOT_A), LOXBOOT_OK);
    CHECK_EQ_INT(ctx.state.slots[0].state, (int)LOXBOOT_SLOT_STATE_INVALID);
    CHECK_EQ_U32(ctx.state.slots[0].firmware_size, 111u);
    CHECK_EQ_U32(ctx.state.slots[0].firmware_crc32, 0xDEADBEEFu);
}

static void test_invalidate_slot_b(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    st.slots[1].firmware_size = 222u;
    st.slots[1].firmware_crc32 = 0x01020304u;
    st.slots[1].record_crc32 = loxboot_crc32((const uint8_t *)&st.slots[1], offsetof(loxboot_slot_record_t, record_crc32));
    st.state_crc32 = state_crc32(&st);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, LOXBOOT_SLOT_B), LOXBOOT_OK);
    CHECK_EQ_INT(ctx.state.slots[1].state, (int)LOXBOOT_SLOT_STATE_INVALID);
    CHECK_EQ_U32(ctx.state.slots[1].firmware_size, 222u);
    CHECK_EQ_U32(ctx.state.slots[1].firmware_crc32, 0x01020304u);
}

static void test_invalidating_corrupt_slot_produces_safe_record(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);

    /* Corrupt slot B record but keep the overall state CRC consistent. */
    st.slots[1].magic = 0u;
    st.slots[1].state = 0xFFu;
    st.slots[1].record_crc32 = 0u;
    st.state_crc32 = state_crc32(&st);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, LOXBOOT_SLOT_B), LOXBOOT_OK);
    CHECK_EQ_U32(ctx.state.slots[1].magic, LOXBOOT_SLOT_MAGIC);
    CHECK_EQ_INT(ctx.state.slots[1].slot_id, (int)LOXBOOT_SLOT_B);
    CHECK_EQ_INT(ctx.state.slots[1].state, (int)LOXBOOT_SLOT_STATE_INVALID);
    CHECK_EQ_U32(ctx.state.slots[1].firmware_size, 0u);
    CHECK_EQ_U32(ctx.state.slots[1].firmware_crc32, 0u);
}

static void test_rejects_invalid_slot_or_uninitialized(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);

    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, LOXBOOT_SLOT_A), LOXBOOT_ERR_INVALID_STATE);
    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, (loxboot_slot_id_t)99), LOXBOOT_ERR_INVALID_STATE);

    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);
    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, (loxboot_slot_id_t)99), LOXBOOT_ERR_INVALID_ARG);
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
    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, LOXBOOT_SLOT_B), LOXBOOT_ERR_FLASH_READ);

    test_seed_state(&flash, &ctx.platform, &st);
    flash.fail_next_erase = true;
    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, LOXBOOT_SLOT_B), LOXBOOT_ERR_FLASH_ERASE);

    test_seed_state(&flash, &ctx.platform, &st);
    flash.fail_next_write = true;
    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, LOXBOOT_SLOT_B), LOXBOOT_ERR_FLASH_WRITE);
}

int main(void)
{
    run_test("invalidate_slot/slot_a", test_invalidate_slot_a);
    run_test("invalidate_slot/slot_b", test_invalidate_slot_b);
    run_test("invalidate_slot/corrupt_slot_produces_safe_record", test_invalidating_corrupt_slot_produces_safe_record);
    run_test("invalidate_slot/rejects_invalid_slot_or_uninitialized", test_rejects_invalid_slot_or_uninitialized);
    run_test("invalidate_slot/propagates_flash_failures", test_propagates_flash_failures);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}

