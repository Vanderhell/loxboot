#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "loxboot/loxboot.h"

#ifdef LOXBOOT_BUILD_UART_PORT
#include "../ports/uart/loxboot_uart.h"
#endif

/* Internal state helpers (implemented in src/loxboot_state.c). */
loxboot_err_t loxboot_state_read(loxboot_t *ctx, loxboot_state_t *out_state);
loxboot_err_t loxboot_state_read_header_crc_only(loxboot_t *ctx, loxboot_state_t *out_state);
loxboot_err_t loxboot_state_write(loxboot_t *ctx, const loxboot_state_t *state);
void loxboot_state_make_default(loxboot_state_t *out, loxboot_slot_id_t active_slot);

static bool loxboot_slot_id_valid(loxboot_slot_id_t slot)
{
    return (slot == LOXBOOT_SLOT_A) || (slot == LOXBOOT_SLOT_B);
}

static bool loxboot_ranges_overlap(uint32_t base_a, uint32_t size_a, uint32_t base_b, uint32_t size_b)
{
    uint32_t end_a = base_a + size_a;
    uint32_t end_b = base_b + size_b;
    return (base_a < end_b) && (base_b < end_a);
}

static uint32_t loxboot_slot_record_crc32(const loxboot_slot_record_t *rec)
{
    return loxboot_crc32((const uint8_t *)rec, offsetof(loxboot_slot_record_t, record_crc32));
}

static void loxboot_slot_record_set_invalid(loxboot_slot_record_t *rec, loxboot_slot_id_t slot)
{
    memset(rec, 0, sizeof(*rec));
    rec->magic = LOXBOOT_SLOT_MAGIC;
    rec->slot_id = slot;
    rec->state = LOXBOOT_SLOT_STATE_INVALID;
    rec->boot_attempts = 0u;
    rec->flags = 0u;
    rec->firmware_size = 0u;
    rec->firmware_crc32 = 0u;
    rec->record_crc32 = loxboot_slot_record_crc32(rec);
}

static void loxboot_slot_record_set_pending(loxboot_slot_record_t *rec,
                                            loxboot_slot_id_t slot,
                                            uint32_t firmware_size,
                                            uint32_t firmware_crc32)
{
    memset(rec, 0, sizeof(*rec));
    rec->magic = LOXBOOT_SLOT_MAGIC;
    rec->slot_id = slot;
    rec->state = LOXBOOT_SLOT_STATE_PENDING;
    rec->boot_attempts = 0u;
    rec->flags = 0u;
    rec->firmware_size = firmware_size;
    rec->firmware_crc32 = firmware_crc32;
    rec->record_crc32 = loxboot_slot_record_crc32(rec);
}

loxboot_err_t loxboot_init(loxboot_t *ctx)
{
    if (ctx == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (ctx->flash.read == NULL || ctx->flash.write == NULL || ctx->flash.erase == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (ctx->hal.on_fatal == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (ctx->platform.slot_size == 0u) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (ctx->platform.slot_a_base == ctx->platform.slot_b_base) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (ctx->platform.boot_state_primary_base == ctx->platform.boot_state_backup_base) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    const uint32_t slot_size = ctx->platform.slot_size;
    const uint32_t state_size = (uint32_t)sizeof(loxboot_state_t);

    if (loxboot_ranges_overlap(ctx->platform.slot_a_base, slot_size,
                              ctx->platform.slot_b_base, slot_size)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (loxboot_ranges_overlap(ctx->platform.boot_state_primary_base, state_size,
                              ctx->platform.boot_state_backup_base, state_size)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (loxboot_ranges_overlap(ctx->platform.boot_state_primary_base, state_size,
                              ctx->platform.slot_a_base, slot_size)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (loxboot_ranges_overlap(ctx->platform.boot_state_primary_base, state_size,
                              ctx->platform.slot_b_base, slot_size)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (loxboot_ranges_overlap(ctx->platform.boot_state_backup_base, state_size,
                              ctx->platform.slot_a_base, slot_size)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (loxboot_ranges_overlap(ctx->platform.boot_state_backup_base, state_size,
                              ctx->platform.slot_b_base, slot_size)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    ctx->initialized = true;
    ctx->boot_reason = LOXBOOT_REASON_UNKNOWN;
    ctx->active_slot = LOXBOOT_SLOT_A;
    memset(&ctx->state, 0, sizeof(ctx->state));
    return LOXBOOT_OK;
}

loxboot_boot_reason_t loxboot_get_boot_reason(const loxboot_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return LOXBOOT_REASON_UNKNOWN;
    }
    return ctx->boot_reason;
}

loxboot_err_t loxboot_get_slot_state(const loxboot_t *ctx,
                                     loxboot_slot_id_t slot,
                                     loxboot_slot_state_t *out_state)
{
    if (ctx == NULL || out_state == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (!loxboot_slot_id_valid(slot)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (!ctx->initialized) {
        return LOXBOOT_ERR_INVALID_STATE;
    }
    *out_state = (loxboot_slot_state_t)ctx->state.slots[slot].state;
    return LOXBOOT_OK;
}

loxboot_err_t loxboot_commit_slot(loxboot_t *ctx,
                                  loxboot_slot_id_t slot,
                                  uint32_t firmware_size,
                                  uint32_t firmware_crc32)
{
    if (ctx == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (!ctx->initialized) {
        return LOXBOOT_ERR_INVALID_STATE;
    }
    if (!loxboot_slot_id_valid(slot)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (firmware_size == 0u) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (firmware_size > ctx->platform.slot_size) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    loxboot_state_t state;
    loxboot_err_t err = loxboot_state_read(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }

    if ((loxboot_slot_id_t)state.active_slot == slot) {
        return LOXBOOT_ERR_INVALID_STATE;
    }

    loxboot_slot_record_set_pending(&state.slots[slot], slot, firmware_size, firmware_crc32);
    state.boot_reason = LOXBOOT_REASON_UNKNOWN;

    err = loxboot_state_write(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }
    ctx->state = state;
    return LOXBOOT_OK;
}

loxboot_err_t loxboot_invalidate_slot(loxboot_t *ctx, loxboot_slot_id_t slot)
{
    if (ctx == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (!ctx->initialized) {
        return LOXBOOT_ERR_INVALID_STATE;
    }
    if (!loxboot_slot_id_valid(slot)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    loxboot_state_t state;
    loxboot_err_t err = loxboot_state_read(ctx, &state);
    if (err != LOXBOOT_OK) {
        if (err != LOXBOOT_ERR_RECORD_CORRUPT) {
            return err;
        }
        /* Allow repairing a corrupt slot record if the state header+CRC are intact. */
        err = loxboot_state_read_header_crc_only(ctx, &state);
        if (err != LOXBOOT_OK) {
            return err;
        }
        loxboot_slot_record_set_invalid(&state.slots[slot], slot);
    } else {
        state.slots[slot].state = LOXBOOT_SLOT_STATE_INVALID;
        state.slots[slot].boot_attempts = 0u;
        state.slots[slot].flags = 0u;
        state.slots[slot].record_crc32 = loxboot_slot_record_crc32(&state.slots[slot]);
    }

    err = loxboot_state_write(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }
    ctx->state = state;
    return LOXBOOT_OK;
}

loxboot_err_t loxboot_format_state(loxboot_t *ctx, loxboot_slot_id_t initial_slot)
{
    if (ctx == NULL || !ctx->initialized) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (initial_slot != LOXBOOT_SLOT_A && initial_slot != LOXBOOT_SLOT_B) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    loxboot_state_t fresh;
    loxboot_state_make_default(&fresh, initial_slot);
    return loxboot_state_write(ctx, &fresh);
}

loxboot_err_t loxboot_confirm_boot(loxboot_t *ctx)
{
    if (ctx == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (!ctx->initialized) {
        return LOXBOOT_ERR_INVALID_STATE;
    }

    loxboot_state_t state;
    loxboot_err_t err = loxboot_state_read(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }

    if (!loxboot_slot_id_valid((loxboot_slot_id_t)state.active_slot)) {
        return LOXBOOT_ERR_INVALID_STATE;
    }

    loxboot_slot_record_t *rec = &state.slots[state.active_slot];
    uint8_t rec_state = rec->state;
    if (rec_state != LOXBOOT_SLOT_STATE_ACTIVE &&
        rec_state != LOXBOOT_SLOT_STATE_VALID) {
        return LOXBOOT_ERR_INVALID_STATE;
    }

    rec->state = LOXBOOT_SLOT_STATE_VALID;
    rec->boot_attempts = 0u;
    rec->flags = 0u;
    rec->record_crc32 = loxboot_slot_record_crc32(rec);

    err = loxboot_state_write(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }
    ctx->state = state;
    return LOXBOOT_OK;
}

loxboot_err_t loxboot_request_slot(loxboot_t *ctx, loxboot_slot_id_t slot)
{
    if (ctx == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (!ctx->initialized) {
        return LOXBOOT_ERR_INVALID_STATE;
    }
    if (!loxboot_slot_id_valid(slot)) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    loxboot_state_t state;
    loxboot_err_t err = loxboot_state_read(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }

    uint8_t target_state = state.slots[slot].state;
    if (target_state != LOXBOOT_SLOT_STATE_VALID &&
        target_state != LOXBOOT_SLOT_STATE_ACTIVE) {
        return LOXBOOT_ERR_INVALID_STATE;
    }

    /* Ensure only one ACTIVE slot is recorded. */
    if (state.active_slot != slot) {
        uint8_t prev = state.active_slot;
        if (prev == LOXBOOT_SLOT_A || prev == LOXBOOT_SLOT_B) {
            if (state.slots[prev].state == LOXBOOT_SLOT_STATE_ACTIVE) {
                state.slots[prev].state = LOXBOOT_SLOT_STATE_VALID;
                state.slots[prev].record_crc32 = loxboot_slot_record_crc32(&state.slots[prev]);
            }
        }
    }

    state.active_slot = slot;
    state.boot_reason = LOXBOOT_REASON_FORCED;
    state.slots[slot].state = LOXBOOT_SLOT_STATE_ACTIVE;
    state.slots[slot].record_crc32 = loxboot_slot_record_crc32(&state.slots[slot]);

    err = loxboot_state_write(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }
    ctx->state = state;
    ctx->boot_reason = LOXBOOT_REASON_FORCED;
    return LOXBOOT_OK;
}

#ifdef LOXBOOT_TEST_HOOKS
static loxboot_jump_hook_t g_jump_hook = NULL;
static void *g_jump_hook_ctx = NULL;
#endif

static void loxboot_jump_to_app(uint32_t slot_base)
{
#ifdef LOXBOOT_TEST_HOOKS
    if (g_jump_hook != NULL) {
        g_jump_hook(g_jump_hook_ctx, slot_base);
        return;
    }
#endif

    volatile uint32_t *p_sp = (volatile uint32_t *)(uintptr_t)slot_base;
    volatile uint32_t *p_entry = (volatile uint32_t *)(uintptr_t)(slot_base + 4u);

    uint32_t sp = *p_sp;
    uint32_t entry = *p_entry;

    typedef void (*loxboot_jump_fn_t)(void);
    loxboot_jump_fn_t app = (loxboot_jump_fn_t)(uintptr_t)(entry | 1u);

#if defined(__arm__) || defined(__thumb__) || defined(__ARM_ARCH)
    __asm volatile ("MSR msp, %0" : : "r"(sp) : );
#else
    (void)sp;
#endif

    app();
}

static loxboot_err_t loxboot_run_rollback(loxboot_t *ctx, loxboot_slot_id_t active_slot)
{
    loxboot_state_t state;
    loxboot_err_t err = loxboot_state_read(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }

    state.slots[active_slot].state = LOXBOOT_SLOT_STATE_ROLLBACK;
    state.slots[active_slot].record_crc32 = loxboot_slot_record_crc32(&state.slots[active_slot]);

    loxboot_slot_id_t fallback_slot = (active_slot == LOXBOOT_SLOT_A) ? LOXBOOT_SLOT_B : LOXBOOT_SLOT_A;
    if (state.slots[fallback_slot].state != LOXBOOT_SLOT_STATE_VALID) {
        return LOXBOOT_ERR_NO_VALID_SLOT;
    }

    state.active_slot = fallback_slot;
    state.boot_reason = LOXBOOT_REASON_ROLLBACK;
    state.slots[fallback_slot].state = LOXBOOT_SLOT_STATE_ACTIVE;
    state.slots[fallback_slot].record_crc32 = loxboot_slot_record_crc32(&state.slots[fallback_slot]);

    err = loxboot_state_write(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }

    ctx->active_slot = fallback_slot;
    ctx->boot_reason = LOXBOOT_REASON_ROLLBACK;
    ctx->state = state;

    return LOXBOOT_OK;
}

loxboot_err_t loxboot_run(loxboot_t *ctx)
{
    if (ctx == NULL || !ctx->initialized) {
        return LOXBOOT_ERR_INVALID_STATE;
    }

    /* [1] Read and validate boot state (dual-copy) */
    loxboot_err_t err = loxboot_state_read(ctx, &ctx->state);
    if (err == LOXBOOT_ERR_RECORD_CORRUPT) {
        /* Fresh flash or total corruption — initialize blank state and retry */
        loxboot_state_t fresh;
        loxboot_state_make_default(&fresh, LOXBOOT_SLOT_A);
        err = loxboot_state_write(ctx, &fresh);
        if (err == LOXBOOT_OK) {
            err = loxboot_state_read(ctx, &ctx->state);
        }
    }
    if (err != LOXBOOT_OK) {
        ctx->hal.on_fatal(ctx->hal.ctx, err);
#ifdef LOXBOOT_TEST_HOOKS
        return LOXBOOT_OK;
#else
        while (1) {}
#endif
    }

#ifdef LOXBOOT_BUILD_UART_PORT
    /* [1.5] UART update session (if transport adapter available) */
    if (ctx->transport.read_byte != NULL && ctx->clock.now_ms != NULL) {
        loxboot_uart_session_t uart_session;
        memset(&uart_session, 0, sizeof(uart_session));
        uart_session.boot = ctx;
        uart_session.listen_ms = (uint32_t)LOXBOOT_UART_LISTEN_MS;

        err = loxboot_uart_run_session(&uart_session);
        if (err != LOXBOOT_OK && err != LOXBOOT_ERR_TIMEOUT) {
            ctx->hal.on_fatal(ctx->hal.ctx, err);
#ifdef LOXBOOT_TEST_HOOKS
            return LOXBOOT_OK;
#else
            while (1) {}
#endif
        }

        err = loxboot_state_read(ctx, &ctx->state);
        if (err != LOXBOOT_OK) {
            ctx->hal.on_fatal(ctx->hal.ctx, err);
#ifdef LOXBOOT_TEST_HOOKS
            return LOXBOOT_OK;
#else
            while (1) {}
#endif
        }
    }
#endif

    /* [2] Determine candidate slot */
boot_retry:
    {
        loxboot_slot_id_t active_slot = (loxboot_slot_id_t)ctx->state.active_slot;
    if (!loxboot_slot_id_valid(active_slot) ||
        ctx->state.slots[active_slot].state == LOXBOOT_SLOT_STATE_EMPTY ||
        ctx->state.slots[active_slot].state == LOXBOOT_SLOT_STATE_INVALID) {

        loxboot_slot_id_t fallback_slot = (active_slot == LOXBOOT_SLOT_A) ? LOXBOOT_SLOT_B : LOXBOOT_SLOT_A;
        if (ctx->state.slots[fallback_slot].state == LOXBOOT_SLOT_STATE_VALID ||
            ctx->state.slots[fallback_slot].state == LOXBOOT_SLOT_STATE_ACTIVE) {
            ctx->state.slots[fallback_slot].state = LOXBOOT_SLOT_STATE_ACTIVE;
            ctx->state.slots[fallback_slot].record_crc32 = loxboot_slot_record_crc32(&ctx->state.slots[fallback_slot]);
            ctx->state.active_slot = fallback_slot;

            err = loxboot_state_write(ctx, &ctx->state);
            if (err != LOXBOOT_OK) {
                ctx->hal.on_fatal(ctx->hal.ctx, err);
#ifdef LOXBOOT_TEST_HOOKS
                return LOXBOOT_OK;
#else
                while (1) {}
#endif
            }
            active_slot = fallback_slot;
        } else {
            ctx->hal.on_fatal(ctx->hal.ctx, LOXBOOT_ERR_NO_VALID_SLOT);
#ifdef LOXBOOT_TEST_HOOKS
            return LOXBOOT_OK;
#else
            while (1) {}
#endif
        }
    }

    /* [3] Check crash loop */
    if (ctx->state.slots[active_slot].boot_attempts >= LOXBOOT_MAX_BOOT_ATTEMPTS) {
        err = loxboot_run_rollback(ctx, active_slot);
        if (err != LOXBOOT_OK) {
            ctx->hal.on_fatal(ctx->hal.ctx, err);
#ifdef LOXBOOT_TEST_HOOKS
            return LOXBOOT_OK;
#else
            while (1) {}
#endif
        }
        active_slot = (loxboot_slot_id_t)ctx->state.active_slot;
        goto boot_retry;
    }

    /* [4] Increment boot_attempts and write state */
    ctx->state.slots[active_slot].boot_attempts++;
    ctx->state.slots[active_slot].record_crc32 = loxboot_slot_record_crc32(&ctx->state.slots[active_slot]);

    err = loxboot_state_write(ctx, &ctx->state);
    if (err != LOXBOOT_OK) {
        ctx->hal.on_fatal(ctx->hal.ctx, err);
#ifdef LOXBOOT_TEST_HOOKS
        return LOXBOOT_OK;
#else
        while (1) {}
#endif
    }

    /* [5] Verify firmware CRC32 */
    uint32_t firmware_size = ctx->state.slots[active_slot].firmware_size;
    uint32_t slot_base = (active_slot == LOXBOOT_SLOT_A) ?
                         ctx->platform.slot_a_base : ctx->platform.slot_b_base;
    uint32_t expected_crc = ctx->state.slots[active_slot].firmware_crc32;

    uint8_t fw_buf[4096];
    uint32_t computed_crc = loxboot_crc32_init();

    if (firmware_size > 0u) {
        uint32_t remaining = firmware_size;
        uint32_t offset = 0u;

        while (remaining > 0u) {
            size_t chunk_size = (remaining > sizeof(fw_buf)) ? sizeof(fw_buf) : (size_t)remaining;
            err = ctx->flash.read(ctx->flash.ctx, slot_base + offset, fw_buf, chunk_size);
            if (err != LOXBOOT_OK) {
                ctx->state.slots[active_slot].state = LOXBOOT_SLOT_STATE_INVALID;
                ctx->state.slots[active_slot].record_crc32 = loxboot_slot_record_crc32(&ctx->state.slots[active_slot]);
                err = loxboot_state_write(ctx, &ctx->state);
                (void)err;
                goto boot_retry;
            }
            computed_crc = loxboot_crc32_update(computed_crc, fw_buf, chunk_size);
            offset += (uint32_t)chunk_size;
            remaining -= (uint32_t)chunk_size;
        }
        computed_crc = loxboot_crc32_finalize(computed_crc);
    }

    if (computed_crc != expected_crc) {
        ctx->state.slots[active_slot].state = LOXBOOT_SLOT_STATE_INVALID;
        ctx->state.slots[active_slot].record_crc32 = loxboot_slot_record_crc32(&ctx->state.slots[active_slot]);

        err = loxboot_state_write(ctx, &ctx->state);
        (void)err;

        goto boot_retry;
    }

    /* [6] Promote PENDING → ACTIVE if needed */
    if (ctx->state.slots[active_slot].state == LOXBOOT_SLOT_STATE_PENDING) {
        ctx->state.slots[active_slot].state = LOXBOOT_SLOT_STATE_ACTIVE;
        ctx->state.slots[active_slot].record_crc32 = loxboot_slot_record_crc32(&ctx->state.slots[active_slot]);

        err = loxboot_state_write(ctx, &ctx->state);
        if (err != LOXBOOT_OK) {
            ctx->hal.on_fatal(ctx->hal.ctx, err);
#ifdef LOXBOOT_TEST_HOOKS
            return LOXBOOT_OK;
#else
            while (1) {}
#endif
        }

        ctx->boot_reason = LOXBOOT_REASON_UPDATE;
    } else if (ctx->boot_reason != LOXBOOT_REASON_ROLLBACK &&
               ctx->boot_reason != LOXBOOT_REASON_FORCED) {
        ctx->boot_reason = LOXBOOT_REASON_NORMAL;
    }

    /* [7] Record boot reason in state */
    ctx->state.boot_reason = ctx->boot_reason;
    ctx->state.slots[active_slot].record_crc32 = loxboot_slot_record_crc32(&ctx->state.slots[active_slot]);

    err = loxboot_state_write(ctx, &ctx->state);
    if (err != LOXBOOT_OK) {
        ctx->hal.on_fatal(ctx->hal.ctx, err);
#ifdef LOXBOOT_TEST_HOOKS
        return LOXBOOT_OK;
#else
        while (1) {}
#endif
    }

    /* [8] Hand off to application */
    ctx->active_slot = active_slot;

    if (ctx->platform_ops.handoff != NULL) {
        /* Platform-specific handoff (e.g. ESP32: esp_ota_set_boot_partition + esp_restart) */
        err = ctx->platform_ops.handoff(ctx->platform_ops.ctx, active_slot);
        /* handoff must not return on success — if it did, treat as fatal */
        ctx->hal.on_fatal(ctx->hal.ctx, (err != LOXBOOT_OK) ? err : LOXBOOT_ERR_INVALID_STATE);
#ifdef LOXBOOT_TEST_HOOKS
        return LOXBOOT_OK;
#else
        while (1) {}
#endif
    }

    /* Default: ARM Cortex-M vector-table jump */
    loxboot_jump_to_app(slot_base);
    }

#ifdef LOXBOOT_TEST_HOOKS
    return LOXBOOT_OK;
#else
    while (1) {}
#endif
}

#ifdef LOXBOOT_TEST_HOOKS
void loxboot_set_jump_hook(loxboot_jump_hook_t hook, void *ctx)
{
    g_jump_hook = hook;
    g_jump_hook_ctx = ctx;
}
#endif
