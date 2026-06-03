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

static void test_request_valid_slot(void)
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
    st.slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_VALID;
    st.slots[1].record_crc32 = slot_record_crc32(&st.slots[1]);
    st.state_crc32 = state_crc32(&st);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_request_slot(&ctx, LOXBOOT_SLOT_B), LOXBOOT_OK);
    CHECK_EQ_INT(ctx.state.active_slot, (int)LOXBOOT_SLOT_B);
    CHECK_EQ_INT(ctx.state.boot_reason, (int)LOXBOOT_REASON_FORCED);
    CHECK_EQ_INT(ctx.state.slots[1].state, (int)LOXBOOT_SLOT_STATE_ACTIVE);
    CHECK_EQ_INT(ctx.state.slots[0].state, (int)LOXBOOT_SLOT_STATE_VALID);
}

static void test_request_active_slot_idempotent(void)
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

    CHECK_EQ_INT(loxboot_request_slot(&ctx, LOXBOOT_SLOT_A), LOXBOOT_OK);
    CHECK_EQ_INT(ctx.state.active_slot, (int)LOXBOOT_SLOT_A);
    CHECK_EQ_INT(ctx.state.slots[0].state, (int)LOXBOOT_SLOT_STATE_ACTIVE);
}

static void test_rejects_nonbootable_states(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_state_t st;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    test_build_default_state(&st, LOXBOOT_SLOT_A);
    st.slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_PENDING;
    st.slots[1].record_crc32 = slot_record_crc32(&st.slots[1]);
    st.state_crc32 = state_crc32(&st);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_request_slot(&ctx, LOXBOOT_SLOT_B), LOXBOOT_ERR_INVALID_STATE);
}

static void test_rejects_invalid_slot_or_uninitialized(void)
{
    loxboot_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    CHECK_EQ_INT(loxboot_request_slot(&ctx, LOXBOOT_SLOT_A), LOXBOOT_ERR_INVALID_STATE);

    test_flash_t flash;
    test_fatal_t fatal;
    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);
    CHECK_EQ_INT(loxboot_request_slot(&ctx, (loxboot_slot_id_t)99), LOXBOOT_ERR_INVALID_ARG);
}

int main(void)
{
    run_test("request_slot/request_valid_slot", test_request_valid_slot);
    run_test("request_slot/request_active_slot_idempotent", test_request_active_slot_idempotent);
    run_test("request_slot/rejects_nonbootable_states", test_rejects_nonbootable_states);
    run_test("request_slot/rejects_invalid_slot_or_uninitialized", test_rejects_invalid_slot_or_uninitialized);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}

