#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "test_support.h"

int g_test_passed = 0;
int g_test_failed = 0;

void test_flash_reset(test_flash_t *flash)
{
    memset(flash->mem, 0xFF, sizeof(flash->mem));
    flash->fail_next_read = false;
    flash->fail_next_write = false;
    flash->fail_next_erase = false;
    flash->fail_erase_on_addr = false;
    flash->fail_write_on_addr = false;
    flash->fail_read_on_addr = false;
    flash->fail_addr = 0u;
}

loxboot_err_t test_flash_read(void *ctx, uint32_t addr, uint8_t *buf, size_t len)
{
    test_flash_t *flash = (test_flash_t *)ctx;
    if (flash->fail_next_read) {
        flash->fail_next_read = false;
        return LOXBOOT_ERR_FLASH_READ;
    }
    if (flash->fail_read_on_addr && addr == flash->fail_addr) {
        flash->fail_read_on_addr = false;
        return LOXBOOT_ERR_FLASH_READ;
    }
    if (buf == NULL || addr > TEST_FLASH_SIZE || len > TEST_FLASH_SIZE || len > (size_t)(TEST_FLASH_SIZE - addr)) {
        return LOXBOOT_ERR_FLASH_READ;
    }
    memcpy(buf, &flash->mem[addr], len);
    return LOXBOOT_OK;
}

loxboot_err_t test_flash_write(void *ctx, uint32_t addr, const uint8_t *buf, size_t len)
{
    test_flash_t *flash = (test_flash_t *)ctx;
    if (flash->fail_next_write) {
        flash->fail_next_write = false;
        return LOXBOOT_ERR_FLASH_WRITE;
    }
    if (flash->fail_write_on_addr && addr == flash->fail_addr) {
        flash->fail_write_on_addr = false;
        return LOXBOOT_ERR_FLASH_WRITE;
    }
    if (buf == NULL || addr > TEST_FLASH_SIZE || len > TEST_FLASH_SIZE || len > (size_t)(TEST_FLASH_SIZE - addr)) {
        return LOXBOOT_ERR_FLASH_WRITE;
    }
    memcpy(&flash->mem[addr], buf, len);
    return LOXBOOT_OK;
}

loxboot_err_t test_flash_erase(void *ctx, uint32_t addr, size_t len)
{
    test_flash_t *flash = (test_flash_t *)ctx;
    if (flash->fail_next_erase) {
        flash->fail_next_erase = false;
        return LOXBOOT_ERR_FLASH_ERASE;
    }
    if (flash->fail_erase_on_addr && addr == flash->fail_addr) {
        flash->fail_erase_on_addr = false;
        return LOXBOOT_ERR_FLASH_ERASE;
    }
    if (addr > TEST_FLASH_SIZE || len > TEST_FLASH_SIZE || len > (size_t)(TEST_FLASH_SIZE - addr)) {
        return LOXBOOT_ERR_FLASH_ERASE;
    }
    memset(&flash->mem[addr], 0xFF, len);
    return LOXBOOT_OK;
}

void test_on_fatal(void *ctx, loxboot_err_t reason)
{
    test_fatal_t *fatal = (test_fatal_t *)ctx;
    fatal->count++;
    fatal->last_reason = reason;
}

static uint32_t slot_record_crc32(const loxboot_slot_record_t *rec)
{
    return loxboot_crc32((const uint8_t *)rec, offsetof(loxboot_slot_record_t, record_crc32));
}

static uint32_t state_crc32(const loxboot_state_t *st)
{
    return loxboot_crc32((const uint8_t *)st, offsetof(loxboot_state_t, state_crc32));
}

void test_build_default_state(loxboot_state_t *out, loxboot_slot_id_t active_slot)
{
    memset(out, 0, sizeof(*out));
    out->magic = LOXBOOT_STATE_MAGIC;
    out->active_slot = (uint8_t)active_slot;
    out->boot_reason = (uint8_t)LOXBOOT_REASON_UNKNOWN;
    out->reserved[0] = 0u;
    out->reserved[1] = 0u;

    out->slots[0].magic = LOXBOOT_SLOT_MAGIC;
    out->slots[0].slot_id = (uint8_t)LOXBOOT_SLOT_A;
    out->slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_EMPTY;
    out->slots[0].boot_attempts = 0u;
    out->slots[0].flags = 0u;
    out->slots[0].firmware_size = 0u;
    out->slots[0].firmware_crc32 = 0u;
    out->slots[0].record_crc32 = slot_record_crc32(&out->slots[0]);

    out->slots[1].magic = LOXBOOT_SLOT_MAGIC;
    out->slots[1].slot_id = (uint8_t)LOXBOOT_SLOT_B;
    out->slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_EMPTY;
    out->slots[1].boot_attempts = 0u;
    out->slots[1].flags = 0u;
    out->slots[1].firmware_size = 0u;
    out->slots[1].firmware_crc32 = 0u;
    out->slots[1].record_crc32 = slot_record_crc32(&out->slots[1]);

    out->state_crc32 = state_crc32(out);
}

void test_seed_state(test_flash_t *flash, const loxboot_platform_t *platform, const loxboot_state_t *state)
{
    (void)test_flash_erase(flash, platform->boot_state_primary_base, sizeof(*state));
    (void)test_flash_write(flash, platform->boot_state_primary_base, (const uint8_t *)state, sizeof(*state));
    (void)test_flash_erase(flash, platform->boot_state_backup_base, sizeof(*state));
    (void)test_flash_write(flash, platform->boot_state_backup_base, (const uint8_t *)state, sizeof(*state));
}

void test_read_state_copy(const test_flash_t *flash, uint32_t base, loxboot_state_t *out_state)
{
    memcpy(out_state, &flash->mem[base], sizeof(*out_state));
}

void test_make_valid_ctx(loxboot_t *ctx, test_flash_t *flash, test_fatal_t *fatal)
{
    memset(ctx, 0, sizeof(*ctx));
    fatal->count = 0;
    fatal->last_reason = LOXBOOT_OK;

    ctx->flash.ctx = flash;
    ctx->flash.read = test_flash_read;
    ctx->flash.write = test_flash_write;
    ctx->flash.erase = test_flash_erase;

    ctx->hal.ctx = fatal;
    ctx->hal.on_fatal = test_on_fatal;

    ctx->platform.boot_state_primary_base = 0x0000u;
    ctx->platform.boot_state_backup_base = 0x0200u;
    ctx->platform.slot_a_base = 0x1000u;
    ctx->platform.slot_b_base = 0x9000u;
    ctx->platform.slot_size = 0x8000u;
}

void run_test(const char *name, test_fn_t fn)
{
    int failed_before = g_test_failed;
    fn();
    if (g_test_failed == failed_before) {
        (void)printf("[PASS] %s\n", name);
    } else {
        (void)printf("[FAIL] %s\n", name);
    }
}
