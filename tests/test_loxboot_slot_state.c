#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "test_support.h"

static void test_null_ctx(void)
{
    loxboot_slot_state_t out = LOXBOOT_SLOT_STATE_EMPTY;
    CHECK_EQ_INT(loxboot_get_slot_state(NULL, LOXBOOT_SLOT_A, &out), LOXBOOT_ERR_INVALID_ARG);
}

static void test_null_out(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    CHECK_EQ_INT(loxboot_get_slot_state(&ctx, LOXBOOT_SLOT_A, NULL), LOXBOOT_ERR_INVALID_ARG);
}

static void test_invalid_slot(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;
    loxboot_slot_state_t out = LOXBOOT_SLOT_STATE_EMPTY;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    CHECK_EQ_INT(loxboot_get_slot_state(&ctx, (loxboot_slot_id_t)7, &out), LOXBOOT_ERR_INVALID_ARG);
}

static void test_uninitialized_ctx(void)
{
    loxboot_t ctx;
    loxboot_slot_state_t out = LOXBOOT_SLOT_STATE_EMPTY;
    memset(&ctx, 0, sizeof(ctx));
    CHECK_EQ_INT(loxboot_get_slot_state(&ctx, LOXBOOT_SLOT_A, &out), LOXBOOT_ERR_INVALID_STATE);
}

static void test_state_after_commit_and_invalidate(void)
{
    test_flash_t flash;
    test_fatal_t fatal;
    loxboot_t ctx;

    test_flash_reset(&flash);
    test_make_valid_ctx(&ctx, &flash, &fatal);
    CHECK_EQ_INT(loxboot_init(&ctx), LOXBOOT_OK);

    loxboot_state_t st;
    test_build_default_state(&st, LOXBOOT_SLOT_A);
    test_seed_state(&flash, &ctx.platform, &st);

    CHECK_EQ_INT(loxboot_commit_slot(&ctx, LOXBOOT_SLOT_B, 1234u, 0x11223344u), LOXBOOT_OK);
    loxboot_slot_state_t out;
    CHECK_EQ_INT(loxboot_get_slot_state(&ctx, LOXBOOT_SLOT_B, &out), LOXBOOT_OK);
    CHECK_EQ_INT(out, LOXBOOT_SLOT_STATE_PENDING);

    CHECK_EQ_INT(loxboot_invalidate_slot(&ctx, LOXBOOT_SLOT_B), LOXBOOT_OK);
    CHECK_EQ_INT(loxboot_get_slot_state(&ctx, LOXBOOT_SLOT_B, &out), LOXBOOT_OK);
    CHECK_EQ_INT(out, LOXBOOT_SLOT_STATE_INVALID);
}

int main(void)
{
    run_test("slot_state/null_ctx", test_null_ctx);
    run_test("slot_state/null_out", test_null_out);
    run_test("slot_state/invalid_slot", test_invalid_slot);
    run_test("slot_state/uninitialized_ctx", test_uninitialized_ctx);
    run_test("slot_state/after_commit_and_invalidate", test_state_after_commit_and_invalidate);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}

