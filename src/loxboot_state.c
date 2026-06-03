#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "loxboot/loxboot.h"

static bool loxboot_slot_state_valid(uint8_t state)
{
    return state <= (uint8_t)LOXBOOT_SLOT_STATE_ROLLBACK;
}

static uint32_t loxboot_slot_record_crc32(const loxboot_slot_record_t *rec)
{
    return loxboot_crc32((const uint8_t *)rec, offsetof(loxboot_slot_record_t, record_crc32));
}

static uint32_t loxboot_state_crc32(const loxboot_state_t *st)
{
    return loxboot_crc32((const uint8_t *)st, offsetof(loxboot_state_t, state_crc32));
}

static bool loxboot_slot_record_is_valid(const loxboot_slot_record_t *rec, uint8_t expected_slot_id)
{
    if (rec->magic != LOXBOOT_SLOT_MAGIC) {
        return false;
    }
    if (rec->slot_id != expected_slot_id) {
        return false;
    }
    if (!loxboot_slot_state_valid(rec->state)) {
        return false;
    }
    if (rec->flags != 0u) {
        return false;
    }
    if (loxboot_slot_record_crc32(rec) != rec->record_crc32) {
        return false;
    }
    return true;
}

static bool loxboot_state_is_valid(const loxboot_state_t *st)
{
    if (st->magic != LOXBOOT_STATE_MAGIC) {
        return false;
    }
    if (st->reserved[0] != 0u || st->reserved[1] != 0u) {
        return false;
    }
    if (loxboot_state_crc32(st) != st->state_crc32) {
        return false;
    }

    if (!loxboot_slot_record_is_valid(&st->slots[0], (uint8_t)LOXBOOT_SLOT_A)) {
        return false;
    }
    if (!loxboot_slot_record_is_valid(&st->slots[1], (uint8_t)LOXBOOT_SLOT_B)) {
        return false;
    }
    return true;
}

static bool loxboot_state_header_crc_valid(const loxboot_state_t *st)
{
    if (st->magic != LOXBOOT_STATE_MAGIC) {
        return false;
    }
    if (st->reserved[0] != 0u || st->reserved[1] != 0u) {
        return false;
    }
    if (loxboot_state_crc32(st) != st->state_crc32) {
        return false;
    }
    return true;
}

static loxboot_err_t loxboot_state_read_copy(loxboot_t *ctx, uint32_t base, loxboot_state_t *out)
{
    loxboot_err_t err = ctx->flash.read(ctx->flash.ctx, base, (uint8_t *)out, sizeof(*out));
    if (err != LOXBOOT_OK) {
        return err;
    }
    if (!loxboot_state_is_valid(out)) {
        return LOXBOOT_ERR_RECORD_CORRUPT;
    }
    return LOXBOOT_OK;
}

static loxboot_err_t loxboot_state_write_copy(loxboot_t *ctx, uint32_t base, const loxboot_state_t *st)
{
    loxboot_err_t err = ctx->flash.erase(ctx->flash.ctx, base, sizeof(*st));
    if (err != LOXBOOT_OK) {
        return err;
    }
    return ctx->flash.write(ctx->flash.ctx, base, (const uint8_t *)st, sizeof(*st));
}

static void loxboot_state_finalize(loxboot_state_t *st)
{
    st->slots[0].record_crc32 = loxboot_slot_record_crc32(&st->slots[0]);
    st->slots[1].record_crc32 = loxboot_slot_record_crc32(&st->slots[1]);
    st->state_crc32 = loxboot_state_crc32(st);
}

loxboot_err_t loxboot_state_read(loxboot_t *ctx, loxboot_state_t *out_state)
{
    loxboot_state_t primary;
    loxboot_state_t backup;

    loxboot_err_t primary_err = loxboot_state_read_copy(ctx, ctx->platform.boot_state_primary_base, &primary);
    if (primary_err == LOXBOOT_OK) {
        *out_state = primary;
        return LOXBOOT_OK;
    }
    if (primary_err != LOXBOOT_ERR_RECORD_CORRUPT) {
        return primary_err;
    }

    loxboot_err_t backup_err = loxboot_state_read_copy(ctx, ctx->platform.boot_state_backup_base, &backup);
    if (backup_err == LOXBOOT_OK) {
        *out_state = backup;
        /* Restore primary from backup to re-establish redundancy. */
        loxboot_err_t restore_err = loxboot_state_write_copy(ctx, ctx->platform.boot_state_primary_base, &backup);
        return (restore_err == LOXBOOT_OK) ? LOXBOOT_OK : restore_err;
    }

    if (backup_err != LOXBOOT_ERR_RECORD_CORRUPT) {
        return backup_err;
    }
    return LOXBOOT_ERR_RECORD_CORRUPT;
}

loxboot_err_t loxboot_state_write(loxboot_t *ctx, const loxboot_state_t *state)
{
    loxboot_state_t tmp;
    memcpy(&tmp, state, sizeof(tmp));
    loxboot_state_finalize(&tmp);

    loxboot_err_t err = loxboot_state_write_copy(ctx, ctx->platform.boot_state_primary_base, &tmp);
    if (err != LOXBOOT_OK) {
        return err;
    }
    err = loxboot_state_write_copy(ctx, ctx->platform.boot_state_backup_base, &tmp);
    if (err != LOXBOOT_OK) {
        return err;
    }
    return LOXBOOT_OK;
}

void loxboot_state_make_default(loxboot_state_t *out, loxboot_slot_id_t active_slot)
{
    memset(out, 0, sizeof(*out));
    out->magic = LOXBOOT_STATE_MAGIC;
    out->active_slot = (uint8_t)active_slot;
    out->boot_reason = (uint8_t)LOXBOOT_REASON_UNKNOWN;

    out->slots[0].magic = LOXBOOT_SLOT_MAGIC;
    out->slots[0].slot_id = (uint8_t)LOXBOOT_SLOT_A;
    out->slots[0].state = (uint8_t)LOXBOOT_SLOT_STATE_EMPTY;
    out->slots[0].boot_attempts = 0u;
    out->slots[0].flags = 0u;
    out->slots[0].firmware_size = 0u;
    out->slots[0].firmware_crc32 = 0u;

    out->slots[1].magic = LOXBOOT_SLOT_MAGIC;
    out->slots[1].slot_id = (uint8_t)LOXBOOT_SLOT_B;
    out->slots[1].state = (uint8_t)LOXBOOT_SLOT_STATE_EMPTY;
    out->slots[1].boot_attempts = 0u;
    out->slots[1].flags = 0u;
    out->slots[1].firmware_size = 0u;
    out->slots[1].firmware_crc32 = 0u;

    loxboot_state_finalize(out);
}

loxboot_err_t loxboot_state_read_header_crc_only(loxboot_t *ctx, loxboot_state_t *out_state)
{
    loxboot_state_t primary;
    loxboot_state_t backup;

    loxboot_err_t err = ctx->flash.read(ctx->flash.ctx, ctx->platform.boot_state_primary_base,
                                        (uint8_t *)&primary, sizeof(primary));
    if (err != LOXBOOT_OK) {
        return err;
    }
    if (loxboot_state_header_crc_valid(&primary)) {
        *out_state = primary;
        return LOXBOOT_OK;
    }

    err = ctx->flash.read(ctx->flash.ctx, ctx->platform.boot_state_backup_base,
                          (uint8_t *)&backup, sizeof(backup));
    if (err != LOXBOOT_OK) {
        return err;
    }
    if (loxboot_state_header_crc_valid(&backup)) {
        *out_state = backup;
        loxboot_err_t restore_err = loxboot_state_write_copy(ctx, ctx->platform.boot_state_primary_base, &backup);
        return (restore_err == LOXBOOT_OK) ? LOXBOOT_OK : restore_err;
    }

    return LOXBOOT_ERR_RECORD_CORRUPT;
}
