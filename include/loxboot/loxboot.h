#ifndef LOXBOOT_H
#define LOXBOOT_H

/**
 * loxboot — Minimal platform-agnostic bootloader core
 *
 * C99, zero-heap, adapter-based.
 *
 * Usage:
 *   1. Fill loxboot_t with adapters and platform config
 *   2. Call loxboot_init() — validates context
 *   3. Call loxboot_run()  — never returns (jumps to app or calls on_fatal)
 *
 * Application side:
 *   Call loxboot_confirm_boot() after successful startup to reset the
 *   crash loop counter and mark the active slot as confirmed.
 *
 * See docs/SPEC.md for full specification.
 * See docs/PORTING.md for platform integration guide.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Version
 * ====================================================================== */

#include "loxboot_version.h"

/* =========================================================================
 * Compile-time configuration
 * ====================================================================== */

/** Maximum consecutive boot attempts before rollback is triggered. */
#ifndef LOXBOOT_MAX_BOOT_ATTEMPTS
#define LOXBOOT_MAX_BOOT_ATTEMPTS 3
#endif

/**
 * UART listen window in milliseconds.
 * loxboot waits this long for a CMD_HELLO before booting normally.
 * Set to 0 to disable UART update check at startup.
 */
#ifndef LOXBOOT_UART_LISTEN_MS
#define LOXBOOT_UART_LISTEN_MS 3000
#endif

/**
 * Maximum UART frame payload size in bytes.
 * Must be large enough for one firmware write chunk (CMD_WRITE payload).
 */
#ifndef LOXBOOT_UART_MAX_FRAME_PAYLOAD
#define LOXBOOT_UART_MAX_FRAME_PAYLOAD 1024
#endif

/* =========================================================================
 * Magic values
 * ====================================================================== */

/** Magic value in loxboot_slot_record_t.magic */
#define LOXBOOT_SLOT_MAGIC  0x4C425354UL  /* "LBST" */

/** Magic value in loxboot_state_t.magic */
#define LOXBOOT_STATE_MAGIC 0x4C585354UL  /* "LXST" */

/* =========================================================================
 * Error codes
 * ====================================================================== */

typedef enum {
    LOXBOOT_OK                 = 0,   /**< Success */
    LOXBOOT_ERR_INVALID_ARG    = 1,   /**< NULL pointer or invalid parameter */
    LOXBOOT_ERR_FLASH_READ     = 2,   /**< Flash read failed */
    LOXBOOT_ERR_FLASH_WRITE    = 3,   /**< Flash write failed */
    LOXBOOT_ERR_FLASH_ERASE    = 4,   /**< Flash erase failed */
    LOXBOOT_ERR_CRC_MISMATCH   = 5,   /**< CRC32 verification failed */
    LOXBOOT_ERR_NO_VALID_SLOT  = 6,   /**< No bootable slot found */
    LOXBOOT_ERR_TIMEOUT        = 7,   /**< Transport receive timeout */
    LOXBOOT_ERR_TRANSPORT      = 8,   /**< Transport protocol error */
    LOXBOOT_ERR_INVALID_STATE  = 9,   /**< Operation invalid in current state */
    LOXBOOT_ERR_RECORD_CORRUPT = 10,  /**< Boot state record CRC invalid */
} loxboot_err_t;

/* =========================================================================
 * Slot identifiers
 * ====================================================================== */

typedef enum {
    LOXBOOT_SLOT_A = 0,
    LOXBOOT_SLOT_B = 1,
} loxboot_slot_id_t;

/* =========================================================================
 * Slot states
 * ====================================================================== */

typedef enum {
    LOXBOOT_SLOT_STATE_EMPTY    = 0,  /**< No firmware present */
    LOXBOOT_SLOT_STATE_PENDING  = 1,  /**< Written, not yet verified */
    LOXBOOT_SLOT_STATE_VALID    = 2,  /**< Verified and confirmed by app */
    LOXBOOT_SLOT_STATE_INVALID  = 3,  /**< CRC failed or explicitly invalidated */
    LOXBOOT_SLOT_STATE_ACTIVE   = 4,  /**< Currently selected for boot */
    LOXBOOT_SLOT_STATE_ROLLBACK = 5,  /**< Demoted after crash loop */
} loxboot_slot_state_t;

/* =========================================================================
 * Boot reason
 * ====================================================================== */

typedef enum {
    LOXBOOT_REASON_NORMAL   = 0,    /**< Normal boot from active/valid slot */
    LOXBOOT_REASON_ROLLBACK = 1,    /**< Crash loop → rolled back to previous slot */
    LOXBOOT_REASON_UPDATE   = 2,    /**< New firmware booted for first time */
    LOXBOOT_REASON_FORCED   = 3,    /**< Requested by application */
    LOXBOOT_REASON_UNKNOWN  = 0xFF, /**< Could not determine reason */
} loxboot_boot_reason_t;

/* =========================================================================
 * Slot record (flash-backed, CRC-protected)
 * ====================================================================== */

/**
 * loxboot_slot_record_t — Per-slot metadata stored in boot state region.
 *
 * record_crc32 covers all fields before it (bytes 0–15, 16 bytes total).
 */
typedef struct {
    uint32_t magic;           /**< Must equal LOXBOOT_SLOT_MAGIC */
    uint8_t  slot_id;         /**< loxboot_slot_id_t */
    uint8_t  state;           /**< loxboot_slot_state_t */
    uint8_t  boot_attempts;   /**< Incremented before boot; reset by confirm */
    uint8_t  flags;           /**< Reserved, must be 0 */
    uint32_t firmware_size;   /**< Firmware image size in bytes */
    uint32_t firmware_crc32;  /**< CRC32 over firmware_size bytes in slot flash */
    uint32_t record_crc32;    /**< CRC32 over bytes 0–15 of this struct */
} loxboot_slot_record_t;

/* =========================================================================
 * Boot state (flash-backed, dual-copy, CRC-protected)
 * ====================================================================== */

/**
 * loxboot_state_t — Complete boot state stored in flash.
 *
 * state_crc32 covers all fields before it.
 *
 * Stored in two copies (primary + backup) at platform-configured addresses.
 */
typedef struct {
    uint32_t              magic;         /**< Must equal LOXBOOT_STATE_MAGIC */
    uint8_t               active_slot;   /**< loxboot_slot_id_t */
    uint8_t               boot_reason;   /**< loxboot_boot_reason_t */
    uint8_t               reserved[2];   /**< Must be 0 */
    loxboot_slot_record_t slots[2];      /**< slots[0]=A, slots[1]=B */
    uint32_t              state_crc32;   /**< CRC32 over all fields above */
} loxboot_state_t;

/* =========================================================================
 * Adapter interfaces
 * ====================================================================== */

/**
 * loxboot_flash_adapter_t — Flash read/write/erase adapter.
 *
 * All functions must be synchronous (blocking).
 * ctx may be NULL if not needed by the implementation.
 */
typedef struct {
    void         *ctx;
    loxboot_err_t (*read) (void *ctx, uint32_t addr, uint8_t *buf, size_t len);
    loxboot_err_t (*write)(void *ctx, uint32_t addr, const uint8_t *buf, size_t len);
    loxboot_err_t (*erase)(void *ctx, uint32_t addr, size_t len);
} loxboot_flash_adapter_t;

/**
 * loxboot_clock_adapter_t — Monotonic millisecond clock adapter.
 *
 * Required only if transport features are used.
 * now_ms is allowed to wrap.
 */
typedef struct {
    void     *ctx;
    uint32_t (*now_ms)(void *ctx);
} loxboot_clock_adapter_t;

/**
 * loxboot_transport_adapter_t — Byte-level transport adapter (UART, etc).
 *
 * Required only if UART update is used.
 * May be zeroed/NULL if transport is not needed.
 */
typedef struct {
    void         *ctx;
    loxboot_err_t (*read_byte) (void *ctx, uint8_t *out, uint32_t timeout_ms);
    loxboot_err_t (*write_byte)(void *ctx, uint8_t b);
    loxboot_err_t (*flush)     (void *ctx);
} loxboot_transport_adapter_t;

/**
 * loxboot_hal_t — Hardware abstraction for fatal error handling.
 *
 * on_fatal MUST NEVER RETURN. If it does, loxboot spins forever.
 */
typedef struct {
    void *ctx;
    void (*on_fatal)(void *ctx, loxboot_err_t reason);
} loxboot_hal_t;

/**
 * loxboot_platform_ops_t — Platform-specific handoff operations.
 *
 * Controls how loxboot hands off execution to an application after boot
 * selection. Different platforms require different mechanisms:
 *
 *   ARM Cortex-M: direct vector-table jump (slot_base + 0/4)
 *   ESP32/Xtensa: esp_ota_set_boot_partition() + esp_restart()
 *   RISC-V:       platform-defined entry point jump
 *
 * If handoff is NULL, loxboot falls back to the built-in ARM Cortex-M
 * vector-table jump. This default is intentionally broken on non-ARM
 * architectures — platforms MUST provide a handoff implementation.
 *
 * handoff MUST NOT RETURN on success. If it returns, loxboot calls on_fatal.
 */
typedef struct {
    void         *ctx;
    /**
     * handoff — Transfer execution to the selected slot.
     *
     * Called by loxboot_run() as the final boot step after CRC verification
     * and state update. Must not return on success.
     *
     * Parameters:
     *   ctx  — platform context (this struct's ctx field)
     *   slot — the slot selected for this boot (SLOT_A or SLOT_B)
     */
    loxboot_err_t (*handoff)(void *ctx, loxboot_slot_id_t slot);
} loxboot_platform_ops_t;

/* =========================================================================
 * Platform configuration
 * ====================================================================== */

/**
 * loxboot_platform_t — Flash address map for this platform.
 *
 * All addresses must be flash-erase-sector aligned.
 * boot_state_primary_base and boot_state_backup_base must be in different sectors.
 */
typedef struct {
    uint32_t boot_state_primary_base; /**< Flash address of primary boot state copy */
    uint32_t boot_state_backup_base;  /**< Flash address of backup boot state copy */
    uint32_t slot_a_base;             /**< Flash address of slot A firmware */
    uint32_t slot_b_base;             /**< Flash address of slot B firmware */
    uint32_t slot_size;               /**< Size of each slot in bytes (must be > 0) */
} loxboot_platform_t;

/* =========================================================================
 * Main context (caller-owned — zero-init before use)
 * ====================================================================== */

/**
 * loxboot_t — Complete loxboot context.
 *
 * Caller allocates (stack or static). Must be zero-initialized before use.
 * All adapter and platform fields must be filled before calling loxboot_init().
 *
 * After loxboot_run() jumps to app (via test hook or real jump), the context
 * may be preserved in .noinit RAM so the app can call loxboot_confirm_boot().
 */
typedef struct {
    loxboot_flash_adapter_t     flash;
    loxboot_clock_adapter_t     clock;
    loxboot_transport_adapter_t transport;
    loxboot_hal_t               hal;
    loxboot_platform_t          platform;
    loxboot_platform_ops_t      platform_ops; /**< Handoff ops — NULL = ARM Cortex-M default */

    /* Runtime state — populated by loxboot_run(), read-only after */
    loxboot_state_t             state;        /**< Current boot state (in-RAM copy) */
    loxboot_slot_id_t           active_slot;  /**< Slot selected for this boot */
    loxboot_boot_reason_t       boot_reason;  /**< Reason for this boot */
    bool                        initialized;  /**< Set by loxboot_init() */
} loxboot_t;

/* =========================================================================
 * Public API
 * ====================================================================== */

/**
 * loxboot_init — Validate context, adapters, and platform config.
 *
 * Must be called before loxboot_run().
 *
 * Checks:
 * - ctx is not NULL
 * - flash.read, flash.write, flash.erase are not NULL
 * - hal.on_fatal is not NULL
 * - platform.slot_size > 0
 * - platform.slot_a_base != platform.slot_b_base
 * - platform.boot_state_primary_base != platform.boot_state_backup_base
 * - slot A and slot B regions do not overlap
 * - boot state regions do not overlap slot A or slot B
 *
 * Does NOT touch flash. Does NOT validate addresses against hardware.
 *
 * Returns: LOXBOOT_OK on success, LOXBOOT_ERR_INVALID_ARG on any failure.
 */
loxboot_err_t loxboot_init(loxboot_t *ctx);

/**
 * loxboot_format_state — Write a blank boot state to flash (factory provisioning).
 *
 * Call once on a fresh device before the first loxboot_run().
 * Writes a valid default state (both slots EMPTY, no firmware committed)
 * to both primary and backup flash regions.
 *
 * Not required on subsequent boots — loxboot_run() detects and handles fresh
 * flash automatically by calling this internally on RECORD_CORRUPT.
 *
 * Parameters:
 *   ctx          — initialized context (loxboot_init must have returned LOXBOOT_OK)
 *   initial_slot — which slot to designate as the initial active slot
 *
 * Returns: LOXBOOT_OK on success, or flash error code on failure.
 */
loxboot_err_t loxboot_format_state(loxboot_t *ctx, loxboot_slot_id_t initial_slot);

/**
 * loxboot_run — Execute the full boot sequence.
 *
 * NOTE: The full boot sequence is implemented in v0.3.0+. In v0.2.0-core,
 * this function returns LOXBOOT_ERR_INVALID_STATE.
 *
 * Sequence:
 *   1. Read and validate boot state from flash (dual-copy)
 *   2. Select active slot (apply crash loop / rollback logic)
 *   3. Increment boot_attempts, write to flash
 *   4. Verify firmware CRC32
 *   5. Record boot reason, write to flash
 *   6. Jump to application
 *
 * On unrecoverable error: calls hal.on_fatal() — never returns.
 * On success: jumps to application — never returns.
 *
 * In test builds (LOXBOOT_TEST_HOOKS=1): jump is intercepted via
 * loxboot_set_jump_hook() instead of executing real jump.
 *
 * Precondition: loxboot_init() must have returned LOXBOOT_OK.
 */
loxboot_err_t loxboot_run(loxboot_t *ctx);

/**
 * loxboot_confirm_boot — Mark the active slot as confirmed.
 *
 * Must be called by the application after successful startup to:
 *   - Set slots[active].boot_attempts = 0
 *   - Set slots[active].state = LOXBOOT_SLOT_STATE_VALID
 *   - Write updated boot state to flash (both copies)
 *
 * If not called: on next boot, boot_attempts will be non-zero.
 * After LOXBOOT_MAX_BOOT_ATTEMPTS consecutive unconfirmed boots: rollback.
 *
 * Returns: LOXBOOT_OK on success.
 *          LOXBOOT_ERR_INVALID_STATE if no active slot found.
 *          LOXBOOT_ERR_FLASH_WRITE if state write fails.
 */
loxboot_err_t loxboot_confirm_boot(loxboot_t *ctx);

/**
 * loxboot_get_boot_reason — Return the reason for the current boot.
 *
 * Valid after loxboot_run() populates ctx->boot_reason.
 * Returns LOXBOOT_REASON_UNKNOWN if context is not initialized.
 */
loxboot_boot_reason_t loxboot_get_boot_reason(const loxboot_t *ctx);

/**
 * loxboot_get_slot_state — Return the current state of a slot.
 *
 * Reads from in-RAM copy of boot state (populated by loxboot_run or last flash read).
 *
 * Returns: LOXBOOT_OK and fills *out_state on success.
 *          LOXBOOT_ERR_INVALID_ARG if ctx or out_state is NULL or slot is invalid.
 *          LOXBOOT_ERR_INVALID_STATE if context is not initialized.
 */
loxboot_err_t loxboot_get_slot_state(const loxboot_t *ctx,
                                      loxboot_slot_id_t slot,
                                      loxboot_slot_state_t *out_state);

/**
 * loxboot_commit_slot — Mark a written slot as PENDING.
 *
 * Called by the transport layer (UART port or application) after writing
 * a complete firmware image into a slot via the flash adapter.
 *
 * Records firmware_size and firmware_crc32 in the slot record.
 * Sets slot state to LOXBOOT_SLOT_STATE_PENDING.
 * Writes updated boot state to flash (both copies).
 *
 * CRC32 verification happens in loxboot_run() on next boot — not here.
 *
 * Returns: LOXBOOT_OK on success.
 *          LOXBOOT_ERR_INVALID_ARG if ctx is NULL or firmware_size is 0.
 *          LOXBOOT_ERR_INVALID_STATE if slot is currently ACTIVE.
 *          LOXBOOT_ERR_FLASH_WRITE if state write fails.
 */
loxboot_err_t loxboot_commit_slot(loxboot_t *ctx,
                                   loxboot_slot_id_t slot,
                                   uint32_t firmware_size,
                                   uint32_t firmware_crc32);

/**
 * loxboot_verify_slot — Verify a slot's firmware against its recorded CRC32.
 *
 * Reads the boot state, then streams firmware_size bytes from the slot's flash
 * region and compares the computed CRC32 with the slot record's firmware_crc32.
 *
 * This is the same integrity check loxboot_run() performs at boot, exposed so
 * "updater" platforms that hand off without calling loxboot_run() (e.g. the
 * ESP32 OTA layer) can reject a corrupt image at commit time.
 *
 * Returns: LOXBOOT_OK if the CRC matches.
 *          LOXBOOT_ERR_INVALID_ARG if ctx is NULL/uninitialised, slot invalid,
 *                                  or the slot records firmware_size == 0.
 *          LOXBOOT_ERR_CRC_MISMATCH if the computed CRC differs.
 *          A flash error code if a slot read fails.
 */
loxboot_err_t loxboot_verify_slot(loxboot_t *ctx, loxboot_slot_id_t slot);

/**
 * loxboot_invalidate_slot — Force a slot to LOXBOOT_SLOT_STATE_INVALID.
 *
 * Used by the transport layer after a write failure or CMD_ABORT.
 * Writes updated boot state to flash (both copies).
 *
 * Returns: LOXBOOT_OK on success.
 *          LOXBOOT_ERR_INVALID_ARG if ctx is NULL or slot is invalid.
 *          LOXBOOT_ERR_FLASH_WRITE if state write fails.
 */
loxboot_err_t loxboot_invalidate_slot(loxboot_t *ctx, loxboot_slot_id_t slot);

/**
 * loxboot_request_slot — Request boot from a specific slot on next reboot.
 *
 * Marks target slot as ACTIVE, sets boot reason to LOXBOOT_REASON_FORCED.
 * Writes updated boot state to flash.
 * Does NOT trigger immediate reboot — caller must reset MCU after calling this.
 *
 * Returns: LOXBOOT_OK on success.
 *          LOXBOOT_ERR_INVALID_ARG if slot is invalid.
 *          LOXBOOT_ERR_INVALID_STATE if target slot is not VALID or ACTIVE.
 *          LOXBOOT_ERR_FLASH_WRITE if state write fails.
 */
loxboot_err_t loxboot_request_slot(loxboot_t *ctx, loxboot_slot_id_t slot);

/**
 * loxboot_crc32 — Standard CRC32 (polynomial 0xEDB88320).
 *
 * Used internally for firmware and record integrity checks.
 * Exposed for use by platform adapters and test code.
 *
 * data: pointer to data buffer (must not be NULL if len > 0)
 * len:  number of bytes
 *
 * Returns: CRC32 value. Returns 0xFFFFFFFF for NULL data with len > 0
 *          (defensive, not a meaningful CRC).
 */
uint32_t loxboot_crc32(const uint8_t *data, size_t len);

/**
 * loxboot_crc32_init — Initialize CRC32 state for incremental computation.
 *
 * Returns the initial CRC32 state for use with loxboot_crc32_update().
 * Use when computing CRC over data received in chunks.
 */
uint32_t loxboot_crc32_init(void);

/**
 * loxboot_crc32_update — Update CRC32 with data chunk.
 *
 * Continues CRC32 computation from previous state.
 * Call loxboot_crc32_init() once, then call this repeatedly with chunks.
 * Call loxboot_crc32_finalize() to get the final CRC.
 *
 * crc: current CRC state (from loxboot_crc32_init or previous call)
 * data: pointer to data chunk (must not be NULL if len > 0)
 * len: number of bytes in chunk
 *
 * Returns: updated CRC state
 */
uint32_t loxboot_crc32_update(uint32_t crc, const uint8_t *data, size_t len);

/**
 * loxboot_crc32_finalize — Finalize CRC32 computation.
 *
 * Completes the CRC32 calculation after all chunks have been processed.
 *
 * crc: final CRC state from loxboot_crc32_update calls
 *
 * Returns: final CRC32 value
 */
uint32_t loxboot_crc32_finalize(uint32_t crc);

/* =========================================================================
 * Test hooks (only available when LOXBOOT_TEST_HOOKS=1)
 * ====================================================================== */

#ifdef LOXBOOT_TEST_HOOKS

/**
 * loxboot_jump_hook_t — Callback intercepting the application jump in tests.
 *
 * Called instead of the real jump when a hook is registered.
 * slot_base: base address of the slot that would be booted.
 */
typedef void (*loxboot_jump_hook_t)(void *ctx, uint32_t slot_base);

/**
 * loxboot_set_jump_hook — Register a jump intercept hook for tests.
 *
 * Must be called before loxboot_run() in test builds.
 * Pass NULL to clear the hook.
 */
void loxboot_set_jump_hook(loxboot_jump_hook_t hook, void *ctx);

#endif /* LOXBOOT_TEST_HOOKS */

#ifdef __cplusplus
}
#endif

#endif /* LOXBOOT_H */
