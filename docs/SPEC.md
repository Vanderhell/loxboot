# loxboot — Full Specification

Version coverage: v0.1.0 through v0.6.0
Status: authoritative

---

## Table of contents

1. Scope and goals
2. Constraints
3. Memory layout
4. Slot model
5. Boot state storage
6. Boot sequence (loxboot_run)
7. Crash loop detection
8. Rollback
9. Boot confirmation
10. Boot reason
11. Firmware update — UART transport protocol
12. Adapter interfaces
13. Error model
14. Test model
15. Platform configuration
16. loxruntime integration (future)
17. Open questions

---

## 1. Scope and goals

loxboot is the bootloader core for the Vanderhell embedded ecosystem.

**Goals:**
- Select a valid firmware slot and jump to it
- Detect crash loops and roll back automatically
- Accept new firmware over UART (and later OTA)
- Record why the system booted (normal, rollback, update, forced)
- Integrate cleanly with panicdump, nvlog, microboot, and loxruntime

**Non-goals:**
- Peripheral initialization (clock, GPIO, UART setup)
- Flash driver implementation (delegated to adapters)
- File system or wear leveling
- Secure boot / firmware signing (future consideration)
- Bootloader self-update

---

## 2. Constraints

| Constraint | Rule |
|---|---|
| Language | C99 only |
| Heap | Zero — no malloc/calloc/realloc/free |
| Dependencies | None — only stdint.h, stddef.h, stdbool.h, string.h |
| Static globals | Forbidden in core — all state in loxboot_t or flash |
| Error handling | Return loxboot_err_t — no abort(), no assert() in production |
| Compiler warnings | Zero on GCC/Clang -Wall -Wextra -Wpedantic and MSVC /W4 |
| Internal leaks | Public headers must not expose internal implementation names |

---

## 3. Memory layout

loxboot assumes the following flash layout. All addresses are provided by the
platform at runtime via `loxboot_platform_t` — loxboot never hard-codes addresses.

```
┌──────────────────────────────────┐  ← flash_base (platform-defined)
│                                  │
│   loxboot image                  │  bootloader code — platform-defined size
│                                  │
├──────────────────────────────────┤
│   Boot state — primary copy      │  sizeof(loxboot_state_t) bytes, CRC-protected
├──────────────────────────────────┤
│   Boot state — backup copy       │  sizeof(loxboot_state_t) bytes, CRC-protected
├──────────────────────────────────┤
│                                  │
│   Slot A                         │  slot_size bytes (platform-defined)
│   (firmware image)               │
│                                  │
├──────────────────────────────────┤
│                                  │
│   Slot B                         │  slot_size bytes (platform-defined)
│   (firmware image)               │
│                                  │
└──────────────────────────────────┘
```

**Platform-provided addresses (via loxboot_platform_t):**

```c
uint32_t boot_state_primary_base;   // address of primary boot state copy
uint32_t boot_state_backup_base;    // address of backup boot state copy
uint32_t slot_a_base;               // address of slot A firmware
uint32_t slot_b_base;               // address of slot B firmware
uint32_t slot_size;                 // size of each slot in bytes
```

**Minimum flash requirements:**
- 2 × sizeof(loxboot_state_t) for boot state (primary + backup)
- 2 × slot_size for firmware slots
- The loxboot image itself (platform-specific, typically 8–32 KB)

---

## 4. Slot model

### Slot identifiers

```c
typedef enum {
    LOXBOOT_SLOT_A = 0,
    LOXBOOT_SLOT_B = 1,
} loxboot_slot_id_t;
```

### Slot states

```c
typedef enum {
    LOXBOOT_SLOT_STATE_EMPTY    = 0,  // No firmware written
    LOXBOOT_SLOT_STATE_PENDING  = 1,  // Written, awaiting verification on next boot
    LOXBOOT_SLOT_STATE_VALID    = 2,  // Verified and confirmed by application
    LOXBOOT_SLOT_STATE_INVALID  = 3,  // CRC failed or explicitly invalidated
    LOXBOOT_SLOT_STATE_ACTIVE   = 4,  // Currently selected for boot
    LOXBOOT_SLOT_STATE_ROLLBACK = 5,  // Demoted after crash loop
} loxboot_slot_state_t;
```

### State transitions

```
EMPTY ──────────────────────────────────────────► PENDING
                                                   (loxboot_commit_slot)
PENDING ──► ACTIVE     (loxboot_run: CRC ok, selected for boot)
PENDING ──► INVALID    (loxboot_run: CRC fail)
ACTIVE  ──► VALID      (loxboot_confirm_boot: app confirmed)
ACTIVE  ──► ROLLBACK   (loxboot_run: crash loop detected)
ROLLBACK──► ACTIVE     (loxboot_run: other slot promoted as fallback)
ANY     ──► INVALID    (loxboot_invalidate_slot: explicit invalidation)
```

### Slot record (flash-backed)

Each slot has one record inside the boot state structure:

```c
typedef struct {
    uint32_t magic;           // Must equal LOXBOOT_SLOT_MAGIC (0x4C425354)
    uint8_t  slot_id;         // loxboot_slot_id_t
    uint8_t  state;           // loxboot_slot_state_t
    uint8_t  boot_attempts;   // Incremented before each boot; reset by confirm
    uint8_t  flags;           // Reserved, must be 0
    uint32_t firmware_size;   // Bytes of firmware image in slot
    uint32_t firmware_crc32;  // CRC32 over firmware_size bytes in slot flash
    uint32_t record_crc32;    // CRC32 over all fields above (16 bytes)
} loxboot_slot_record_t;      // Total: 20 bytes
```

`record_crc32` covers bytes 0–15 of the struct (all fields before it).

---

## 5. Boot state storage

The boot state region holds both slot records and a global header.

### Boot state structure

```c
#define LOXBOOT_STATE_MAGIC 0x4C585354UL  /* "LXST" */

typedef struct {
    uint32_t              magic;
    uint8_t               active_slot;      // loxboot_slot_id_t
    uint8_t               boot_reason;      // loxboot_boot_reason_t
    uint8_t               reserved[2];
    loxboot_slot_record_t slots[2];         // slots[0]=A, slots[1]=B
    uint32_t              state_crc32;      // CRC32 over all fields above
} loxboot_state_t;
```

### Dual-copy strategy

The boot state is stored in two flash copies: primary and backup.

**Read strategy:**
1. Read primary copy from flash
2. Validate `state_crc32`
3. If primary is valid → use it
4. If primary is invalid → read backup copy
5. If backup is valid → use it, restore primary from backup
6. If both are invalid:
   - v0.2.0-core slot-control functions return `LOXBOOT_ERR_RECORD_CORRUPT`
   - v0.3.0+ `loxboot_run()` calls `hal.on_fatal(LOXBOOT_ERR_RECORD_CORRUPT)`

**Write strategy:**
1. Write primary copy with updated CRC
2. Write backup copy
3. If either write fails → return `LOXBOOT_ERR_FLASH_WRITE`

This ensures that a power-loss during write cannot corrupt both copies simultaneously
(assuming primary and backup are in different flash sectors/pages).

---

## 6. Boot sequence (loxboot_run)

`loxboot_run()` is the main entry point. It never returns in production
(either jumps to app or calls `hal.on_fatal()`).

```
loxboot_run(ctx)
│
├─ [1] Read and validate boot state (dual-copy)
│       └─ Both copies corrupt → on_fatal(LOXBOOT_ERR_RECORD_CORRUPT)
│
├─ [2] Determine candidate slot
│       Active slot = ctx->state.active_slot
│       If no active slot → find VALID slot → promote to ACTIVE
│       If no VALID slot either → on_fatal(LOXBOOT_ERR_NO_VALID_SLOT)
│
├─ [3] Check crash loop
│       If slots[active].boot_attempts >= LOXBOOT_MAX_BOOT_ATTEMPTS
│         → trigger rollback (see §8)
│
├─ [4] Increment boot_attempts on active slot
│       Write updated boot state to flash (both copies)
│       If write fails → on_fatal(LOXBOOT_ERR_FLASH_WRITE)
│
├─ [5] Verify firmware CRC32
│       Read slot firmware from flash (firmware_size bytes at slot base)
│       Compute CRC32
│       Compare to slots[active].firmware_crc32
│       If mismatch → mark slot INVALID, write state, retry from [2]
│       If slot also had PENDING state → this was a new update that arrived corrupt
│
├─ [6] Promote PENDING → ACTIVE (if selected slot was PENDING and CRC ok)
│       Write updated boot state
│
├─ [7] Record boot reason in state
│       Write boot reason to state.boot_reason
│       Write updated boot state
│
├─ [8] Jump to application
│       Compute app_entry = slot_base + 4 (reset handler from vector table)
│       Set stack pointer from slot_base[0]
│       Jump to slot_base[1] (reset handler)
│       (never returns)
│
└─ All on_fatal paths: call ctx->hal.on_fatal(ctx->hal.ctx, reason)
                        on_fatal must never return
                        if it returns anyway: spin forever
```

### Jump implementation (platform-agnostic default)

```c
typedef void (*loxboot_jump_fn_t)(void);

static void loxboot_jump_to_app(uint32_t slot_base) {
    /* Read stack pointer and reset handler from vector table */
    uint32_t sp    = *(volatile uint32_t *)(slot_base + 0);
    uint32_t entry = *(volatile uint32_t *)(slot_base + 4);

    /* Cast entry to function pointer and call */
    loxboot_jump_fn_t app = (loxboot_jump_fn_t)(entry | 1U); /* Thumb bit */

    /* Set stack pointer (Cortex-M) */
    /* NOTE: platform adapters may override this for non-Cortex-M */
    __asm volatile ("MSR msp, %0" : : "r"(sp) : );

    app(); /* never returns */
}
```

For test builds (`LOXBOOT_TEST_HOOKS=1`), the jump is intercepted via:

```c
typedef void (*loxboot_jump_hook_t)(void *ctx, uint32_t slot_base);
void loxboot_set_jump_hook(loxboot_jump_hook_t hook, void *ctx);
```

---

## 7. Crash loop detection

loxboot counts boot attempts per slot.

**Counter lifecycle:**
- Before each boot attempt: `boot_attempts++` written to flash
- After successful app startup: application calls `loxboot_confirm_boot()` → `boot_attempts = 0`
- If the application never calls `loxboot_confirm_boot()` (crash before confirmation): counter remains incremented

**Threshold:** `LOXBOOT_MAX_BOOT_ATTEMPTS` (default: 3, compile-time configurable)

**Detection:** at step [3] of boot sequence:
```
if (slots[active].boot_attempts >= LOXBOOT_MAX_BOOT_ATTEMPTS)
    → trigger rollback
```

**Compatibility note:** This model is the same as MCUboot's boot counter model
and is compatible with `microboot` boot loop detection. Both can coexist: `microboot`
detects loops in the application layer; loxboot detects them at the bootloader layer.

---

## 8. Rollback

Triggered when crash loop is detected OR when active slot CRC fails.

**Rollback procedure:**

```
1. Mark active slot as LOXBOOT_SLOT_STATE_ROLLBACK
2. Search for fallback slot:
   - Other slot (if active=A → check B, if active=B → check A)
   - Fallback is eligible if state == VALID
3. If fallback found:
   - Promote fallback to ACTIVE
   - Update state.active_slot
   - Write updated boot state (both copies)
   - Set boot_reason = LOXBOOT_REASON_ROLLBACK
   - Continue boot sequence with fallback slot
4. If no fallback:
   - on_fatal(LOXBOOT_ERR_NO_VALID_SLOT)
```

**Important:** Rollback does not erase the demoted slot. The slot remains in
ROLLBACK state. The application can inspect it after boot via `loxboot_get_slot_state()`.

---

## 9. Boot confirmation

The application must call `loxboot_confirm_boot()` after successful startup.

**What it does:**
```
1. Read current boot state from flash
2. Find active slot
3. Set slots[active].boot_attempts = 0
4. Set slots[active].state = LOXBOOT_SLOT_STATE_VALID
5. Write updated boot state (both copies)
6. Return LOXBOOT_OK
```

**When to call:** As early as possible after the application has verified its
own health. Calling it too early risks confirming a broken firmware.
Calling it too late risks a false crash-loop rollback.

**Application pattern:**
```c
// After RTOS init, peripheral init, self-test:
loxboot_err_t err = loxboot_confirm_boot(&loxboot_ctx);
if (err != LOXBOOT_OK) {
    // Log the error but do not halt — app is running
}
```

---

## 10. Boot reason

```c
typedef enum {
    LOXBOOT_REASON_NORMAL   = 0,   // Normal boot from active/valid slot
    LOXBOOT_REASON_ROLLBACK = 1,   // Crash loop → rolled back to previous slot
    LOXBOOT_REASON_UPDATE   = 2,   // New firmware booted for first time (PENDING→ACTIVE)
    LOXBOOT_REASON_FORCED   = 3,   // Application requested boot into specific slot
    LOXBOOT_REASON_UNKNOWN  = 0xFF // Could not determine reason
} loxboot_boot_reason_t;
```

Boot reason is written to `loxboot_state_t.boot_reason` during `loxboot_run()`.
The application reads it via `loxboot_get_boot_reason()`.

**Reason assignment rules:**
- New firmware selected from PENDING state → `LOXBOOT_REASON_UPDATE`
- Rollback triggered → `LOXBOOT_REASON_ROLLBACK`
- Normal boot from VALID/ACTIVE slot, no rollback → `LOXBOOT_REASON_NORMAL`
- Application called `loxboot_request_slot()` → `LOXBOOT_REASON_FORCED`
- Boot state corrupt and recovered → `LOXBOOT_REASON_UNKNOWN`

---

## 11. Firmware update — UART transport protocol

Implemented in `ports/uart/loxboot_uart.c`. Uses `loxboot_transport_adapter_t`.

### Frame format

Every message (command and response) uses the same frame structure:

```
┌──────┬──────┬──────────┬──────────┬─────────────────────┬──────────┐
│ SOF  │ CMD  │ LEN_LO   │ LEN_HI   │ PAYLOAD (0..LEN-1)  │ CRC16    │
│ 0x7E │ 1B   │ 1B       │ 1B       │ LEN bytes           │ 2B LE    │
└──────┴──────┴──────────┴──────────┴─────────────────────┴──────────┘
```

- SOF: `0x7E` (fixed)
- CMD: command/response byte (see table below)
- LEN: 16-bit little-endian payload length (0 = no payload)
- PAYLOAD: LEN bytes of payload data
- CRC16: CRC16-CCITT (poly 0x1021, init 0xFFFF) over CMD+LEN_LO+LEN_HI+PAYLOAD

Maximum payload size: 1024 bytes (compile-time: `LOXBOOT_UART_MAX_FRAME_PAYLOAD`)

### Commands (host → device)

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x01 | CMD_HELLO | none | Start session, device replies with STATUS |
| 0x02 | CMD_WRITE | offset(4B) + data(N) | Write N bytes at offset into target slot |
| 0x03 | CMD_COMMIT | size(4B) + crc32(4B) | Finalize write, verify CRC, mark PENDING |
| 0x04 | CMD_ABORT | none | Abort update, invalidate slot |
| 0x05 | CMD_STATUS | none | Request current slot status |
| 0x06 | CMD_REBOOT | none | Request immediate reboot |

### Responses (device → host)

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x81 | RSP_OK | none | Last command accepted |
| 0x82 | RSP_ERROR | err(1B) | Last command failed, err = loxboot_err_t |
| 0x83 | RSP_STATUS | see below | Response to CMD_HELLO and CMD_STATUS |

RSP_STATUS payload:
```
slot_a_state (1B) | slot_b_state (1B) | active_slot (1B) | boot_reason (1B)
```

### UART update sequence

```
Host                                    Device (loxboot)
 │                                           │
 │── CMD_HELLO ──────────────────────────► │
 │ ◄── RSP_STATUS ────────────────────────  │
 │                                           │
 │── CMD_WRITE (offset=0, data[0..1023]) ─► │
 │ ◄── RSP_OK ────────────────────────────  │
 │── CMD_WRITE (offset=1024, data[...]) ──► │
 │ ◄── RSP_OK ────────────────────────────  │
 │  ... repeat until all bytes written ...  │
 │                                           │
 │── CMD_COMMIT (size, crc32) ────────────► │
 │ ◄── RSP_OK ────────────────────────────  │  (slot marked PENDING)
 │                                           │
 │── CMD_REBOOT ──────────────────────────► │
 │ ◄── RSP_OK ────────────────────────────  │  (device reboots)
 │                                           │
 │  [device reboots, loxboot_run verifies]   │
```

On reboot, `loxboot_run()` finds the PENDING slot, verifies CRC, promotes to ACTIVE, boots.

### Error handling in transport

- CRC16 mismatch on received frame → device sends RSP_ERROR(LOXBOOT_ERR_TRANSPORT), discards frame
- CMD_WRITE out of bounds (offset + len > slot_size) → RSP_ERROR(LOXBOOT_ERR_INVALID_ARG)
- CMD_COMMIT CRC32 mismatch → RSP_ERROR(LOXBOOT_ERR_CRC_MISMATCH), slot marked INVALID
- Any flash write error → RSP_ERROR(LOXBOOT_ERR_FLASH_WRITE), slot marked INVALID

### Target slot selection

The inactive slot is always the update target. loxboot_uart determines it as:
```
target_slot = (active_slot == LOXBOOT_SLOT_A) ? LOXBOOT_SLOT_B : LOXBOOT_SLOT_A;
```

If both slots are in an unknown state (first boot), Slot B is the default update target.

### Timeout behavior

- Per-byte read timeout: `loxboot_transport_session_t.timeout_ms` (default: 5000 ms)
- If timeout expires during frame receive → session ends, slot invalidated
- The bootloader checks for update request only during a configurable window at startup
  (default: 3 seconds, checking for CMD_HELLO). If no CMD_HELLO received → boot normally.

Update check window: `LOXBOOT_UART_LISTEN_MS` (default: 3000, compile-time configurable)

---

## 12. Adapter interfaces

### Flash adapter

```c
typedef struct {
    void         *ctx;
    loxboot_err_t (*read) (void *ctx, uint32_t addr, uint8_t *buf, size_t len);
    loxboot_err_t (*write)(void *ctx, uint32_t addr, const uint8_t *buf, size_t len);
    loxboot_err_t (*erase)(void *ctx, uint32_t addr, size_t len);
} loxboot_flash_adapter_t;
```

Contracts:
- `read`: copies `len` bytes from flash address `addr` into `buf`. Returns LOXBOOT_OK or LOXBOOT_ERR_FLASH_READ.
- `write`: writes `len` bytes from `buf` into flash at `addr`. Caller must ensure sector is erased. Returns LOXBOOT_OK or LOXBOOT_ERR_FLASH_WRITE.
- `erase`: erases flash region starting at `addr` of `len` bytes. `addr` and `len` must be aligned to erase sector size (platform-specific). Returns LOXBOOT_OK or LOXBOOT_ERR_FLASH_ERASE.
- All three: `ctx` may be NULL if not needed by the implementation.
- All three: must be reentrant if called from multiple contexts (not required by loxboot core — single-threaded by design).

### Clock adapter

```c
typedef struct {
    void     *ctx;
    uint32_t (*now_ms)(void *ctx);
} loxboot_clock_adapter_t;
```

Contract:
- `now_ms`: returns a monotonically increasing millisecond timestamp. Allowed to wrap. Used only for transport timeout comparison.
- May be NULL if transport features are not used.

### Transport adapter

```c
typedef struct {
    void         *ctx;
    loxboot_err_t (*read_byte) (void *ctx, uint8_t *out, uint32_t timeout_ms);
    loxboot_err_t (*write_byte)(void *ctx, uint8_t b);
    loxboot_err_t (*flush)     (void *ctx);
} loxboot_transport_adapter_t;
```

Contracts:
- `read_byte`: blocking read of one byte, timeout in ms. Returns LOXBOOT_OK, LOXBOOT_ERR_TIMEOUT, or LOXBOOT_ERR_TRANSPORT.
- `write_byte`: blocking write of one byte. Returns LOXBOOT_OK or LOXBOOT_ERR_TRANSPORT.
- `flush`: flush any buffered output. May be a no-op. Returns LOXBOOT_OK or LOXBOOT_ERR_TRANSPORT.
- May be NULL if transport features are not used.

### HAL

```c
typedef struct {
    void *ctx;
    void (*on_fatal)(void *ctx, loxboot_err_t reason);
} loxboot_hal_t;
```

Contract:
- `on_fatal`: called on unrecoverable errors. **Must never return.** Typical implementations spin forever, reset the MCU, or enter a safe low-power state.
- If `on_fatal` returns (e.g. in tests with a mock), loxboot spins in a tight loop.

---

## 13. Error model

```c
typedef enum {
    LOXBOOT_OK                 = 0,
    LOXBOOT_ERR_INVALID_ARG    = 1,   // NULL pointer or invalid parameter
    LOXBOOT_ERR_FLASH_READ     = 2,   // Flash read operation failed
    LOXBOOT_ERR_FLASH_WRITE    = 3,   // Flash write operation failed
    LOXBOOT_ERR_FLASH_ERASE    = 4,   // Flash erase operation failed
    LOXBOOT_ERR_CRC_MISMATCH   = 5,   // CRC32 verification failed
    LOXBOOT_ERR_NO_VALID_SLOT  = 6,   // No bootable slot found
    LOXBOOT_ERR_TIMEOUT        = 7,   // Transport timeout
    LOXBOOT_ERR_TRANSPORT      = 8,   // Transport protocol error
    LOXBOOT_ERR_INVALID_STATE  = 9,   // Operation not valid in current state
    LOXBOOT_ERR_RECORD_CORRUPT = 10,  // Boot state record CRC invalid
} loxboot_err_t;
```

Rules:
- Every function that can fail returns `loxboot_err_t`
- `LOXBOOT_OK` (0) always means success
- Errors are not accumulated — each call site handles its own error
- `on_fatal` is called only when no recovery is possible and the boot cannot proceed
- `loxboot_run()` calls `on_fatal` internally — callers of `loxboot_run()` never see a return value on fatal paths

---

## 14. Test model

### What must be tested

| Area | Test file | Method |
|---|---|---|
| CRC32 known vectors | test_loxboot_crc32.c | Direct function call |
| CRC32 empty input | test_loxboot_crc32.c | Direct function call |
| init: all adapters valid | test_loxboot_init.c | RAM adapter |
| init: NULL flash | test_loxboot_init.c | NULL injection |
| init: NULL hal | test_loxboot_init.c | NULL injection |
| init: zero slot_size | test_loxboot_init.c | Zero value |
| slot state: get after commit | test_loxboot_commit_slot.c | RAM adapter |
| slot state: get after invalidate | test_loxboot_invalidate_slot.c | RAM adapter |
| confirm_boot: resets attempts | test_loxboot_confirm_boot.c | RAM adapter |
| confirm_boot: no active slot | test_loxboot_confirm_boot.c | RAM adapter |
| boot sequence: normal boot | test_loxboot_boot_sequence.c | RAM + jump hook |
| boot sequence: PENDING→ACTIVE | test_loxboot_boot_sequence.c | RAM + jump hook |
| boot sequence: CRC fail | test_loxboot_boot_sequence.c | RAM + jump hook |
| crash loop: detect at threshold | test_loxboot_crash_loop.c | RAM + jump hook |
| crash loop: reset after confirm | test_loxboot_crash_loop.c | RAM + jump hook |
| rollback: to valid slot | test_loxboot_rollback.c | RAM + jump hook |
| rollback: no fallback → fatal | test_loxboot_rollback.c | RAM + mock fatal |
| dual-copy: primary corrupt | test_loxboot_boot_sequence.c | RAM corruption |
| dual-copy: both corrupt → fatal | test_loxboot_boot_sequence.c | RAM + mock fatal |
| UART: frame encode | test_loxboot_uart_frame.c | Direct |
| UART: frame decode valid | test_loxboot_uart_frame.c | Direct |
| UART: frame decode CRC fail | test_loxboot_uart_frame.c | Direct |
| UART: receive full image | test_loxboot_uart_receive.c | Mock transport |
| UART: receive abort | test_loxboot_uart_receive.c | Mock transport |
| UART: receive timeout | test_loxboot_uart_receive.c | Mock transport |

### What is NOT tested in CTest

- STM32 flash adapter (hardware-only)
- ESP32 flash adapter (hardware-only)
- Actual jump execution (intercepted via jump hook)
- UART peripheral timing

---

## 15. Platform configuration

`loxboot_platform_t` is filled by the platform before calling `loxboot_init()`:

```c
typedef struct {
    uint32_t boot_state_primary_base;   // Flash address of primary boot state copy
    uint32_t boot_state_backup_base;    // Flash address of backup boot state copy
    uint32_t slot_a_base;               // Flash address of slot A firmware
    uint32_t slot_b_base;               // Flash address of slot B firmware
    uint32_t slot_size;                 // Size of each slot in bytes (must be > 0)
} loxboot_platform_t;
```

### Example: STM32F4 with 1MB flash

```c
loxboot_platform_t platform = {
    .boot_state_primary_base = 0x08004000,  // Sector 1 (16KB)
    .boot_state_backup_base  = 0x08008000,  // Sector 2 (16KB)
    .slot_a_base             = 0x08020000,  // Sectors 5–7 (128KB each)
    .slot_b_base             = 0x08060000,  // Sectors 8–10
    .slot_size               = 0x00040000,  // 256KB per slot
};
```

### Example: Generic (tests)

```c
loxboot_platform_t platform = {
    .boot_state_primary_base = 0x00000000,
    .boot_state_backup_base  = 0x00000100,
    .slot_a_base             = 0x00001000,
    .slot_b_base             = 0x00009000,
    .slot_size               = 0x00008000,  // 32KB per slot
};
```

---

## 16. loxruntime integration (future — v0.7.0+)

When loxruntime gains a `vh_boot` facade, loxboot becomes its backend.

The integration is an adapter layer (not part of loxboot itself):

```c
// loxruntime/adapters/vh_boot_loxboot.c (future)
// Wraps loxboot_t behind vh_boot_t adapter

vh_err_t vh_boot_confirm(vh_boot_t *ctx) {
    loxboot_t *lb = (loxboot_t *)ctx->impl;
    loxboot_err_t err = loxboot_confirm_boot(lb);
    return (err == LOXBOOT_OK) ? VH_OK : VH_ERR_INVALID_STATE;
}
```

loxboot itself has no dependency on loxruntime.

**Ecosystem integration points (available now, not required):**

| loxboot event | Ecosystem library | Integration |
|---|---|---|
| Boot reason = ROLLBACK | nvlog | Log rollback event |
| Boot reason = ROLLBACK | panicdump | Read crash dump from previous boot |
| boot_attempts | microboot | Cross-validate with app-level counter |
| Firmware written | vh_evidence (loxruntime) | Record update evidence |

---

## 17. Open questions

These must be resolved by the project owner before v0.2.0 implementation begins.

| # | Question | Recommendation |
|---|---|---|
| 1 | Boot state: single copy or dual copy? | Dual copy (safer on power-loss) |
| 2 | CRC32 polynomial: standard or configurable? | Standard 0xEDB88320, not configurable |
| 3 | Jump mechanism: generic or Cortex-M specific? | Generic function pointer for v0.2.0; Cortex-M specific in STM32 adapter (v0.5.0) |
| 4 | UART protocol: custom or XMODEM? | Custom (defined in §11) |
| 5 | Boot state region size: fixed or configurable? | Fixed to sizeof(loxboot_state_t) per copy in v0.2.0-core |
| 6 | UART listen window: always-on or timed? | Timed (LOXBOOT_UART_LISTEN_MS = 3000 ms) |
| 7 | Slot erase on write: caller responsibility or loxboot_commit_slot? | Caller erases before writing; loxboot_commit_slot only writes metadata |
