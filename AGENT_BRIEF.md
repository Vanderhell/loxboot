# loxboot — Agent Implementation Brief

This document is the **authoritative implementation guide** for loxboot.
An agent must read this entire file before writing any code.

---

## What you are building

`loxboot` is a C99, zero-heap, platform-agnostic bootloader core for bare-metal MCUs.
It lives at `github.com/Vanderhell/loxboot`.

It handles everything that must happen before the main application runs:
- Slot selection (A/B model)
- Firmware integrity verification (CRC32)
- Crash loop detection and automatic rollback
- Boot reason recording
- Firmware update receive (UART transport)
- Jump to application

It does NOT implement flash drivers, UART peripheral registers, or any hardware-specific code.
All hardware access is injected via adapter structs (function pointers).

---

## Non-negotiable constraints

These apply to every file in `include/` and `src/`:

1. **C99 only.** No C11, no C++, no compiler extensions except `__attribute__((noreturn))` where needed.
2. **Zero heap.** No `malloc`, `calloc`, `realloc`, `free` — anywhere, ever.
3. **Zero dependencies.** No RTOS, no HAL, no external libraries. Only `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`, `<string.h>` from libc.
4. **No internal name leaks.** Public headers must not mention internal implementation details.
5. **No static globals in core.** All state is either caller-owned (`loxboot_t`) or flash-backed.
6. **All errors returned.** No abort(), no assert() in production code. Only `hal.on_fatal()` for truly unrecoverable situations.
7. **Every public function documented** with a contract comment in the header.
8. **-Wall -Wextra -Wpedantic** must produce zero warnings on GCC and Clang.
9. **MSVC /W4** must produce zero warnings.
10. **Every implemented function has at least one test.**

---

## Repository layout (final state)

```
loxboot/
├── include/loxboot/
│   ├── loxboot.h                   # Main public API
│   ├── loxboot_transport.h         # Transport adapter interface
│   └── loxboot_version.h           # Version defines
├── src/
│   ├── loxboot_core.c              # Boot sequence, slot logic, CRC
│   ├── loxboot_crc32.c             # CRC32 implementation
│   └── loxboot_state.c             # Boot state read/write/validate
├── ports/
│   └── uart/
│       ├── loxboot_uart.h          # UART port public header
│       └── loxboot_uart.c          # UART frame protocol implementation
├── adapters/
│   ├── stm32/
│   │   ├── loxboot_flash_stm32.h
│   │   └── loxboot_flash_stm32.c
│   └── esp32/
│       ├── loxboot_flash_esp32.h
│       └── loxboot_flash_esp32.c
├── tests/
│   ├── test_loxboot_crc32.c
│   ├── test_loxboot_init.c
│   ├── test_loxboot_slot_state.c
│   ├── test_loxboot_confirm_boot.c
│   ├── test_loxboot_commit_slot.c
│   ├── test_loxboot_invalidate_slot.c
│   ├── test_loxboot_boot_sequence.c
│   ├── test_loxboot_rollback.c
│   ├── test_loxboot_crash_loop.c
│   ├── test_loxboot_uart_frame.c
│   └── test_loxboot_uart_receive.c
├── docs/
│   ├── SPEC.md                     # Full specification (all versions)
│   ├── MEMORY_LAYOUT.md            # Memory layout guide
│   ├── PORTING.md                  # How to write a flash adapter
│   └── INTEGRATION.md              # How to integrate with loxruntime
├── CMakeLists.txt
├── EVIDENCE_MATRIX.md
├── PROJECT_STATE.md
├── README.md
└── LICENSE
```

---

## Implementation order (mandatory)

Do not skip steps. Do not implement a later step before an earlier one is complete and tested.

### Step 1 — loxboot_crc32 (src/loxboot_crc32.c)
- Implement `loxboot_crc32(const uint8_t *data, size_t len)`
- Standard CRC32, polynomial 0xEDB88320, table-driven
- Table is a static const array (not generated at runtime — no heap)
- Test: `tests/test_loxboot_crc32.c` — known vectors, NULL safety, empty input

### Step 2 — loxboot_init (src/loxboot_core.c)
- Validate all required adapter function pointers
- Validate platform addresses (slot_size > 0)
- Return LOXBOOT_ERR_INVALID_ARG on any NULL or invalid field
- Test: `tests/test_loxboot_init.c` — all NULL combinations, valid config

### Step 3 — Boot state read/write (src/loxboot_state.c)
- `loxboot_state_read()` — read from flash, validate CRC, populate slot_records
- `loxboot_state_write()` — compute CRC, write to flash
- Internal functions only — not exported in public header
- Test: via `tests/test_loxboot_slot_state.c` using RAM flash adapter

### Step 4 — Slot control API
- `loxboot_get_slot_state()`
- `loxboot_commit_slot()`
- `loxboot_invalidate_slot()`
- `loxboot_confirm_boot()`
- All write updated state to flash via state write
- Tests: individual test files per function

### Step 5 — Boot sequence (loxboot_run)
- Full sequence per SPEC.md §4
- Uses RAM flash adapter in tests — never calls real flash
- Test: `tests/test_loxboot_boot_sequence.c`, `test_loxboot_rollback.c`, `test_loxboot_crash_loop.c`
- NOTE: loxboot_run() jumps to application and never returns in production.
  In tests, the jump is intercepted via a mock HAL jump hook.

### Step 6 — UART transport port (ports/uart/)
- Frame protocol per SPEC.md §8
- Uses transport adapter only — no direct UART register access
- Test: `tests/test_loxboot_uart_frame.c`, `tests/test_loxboot_uart_receive.c`

### Step 7 — STM32 flash adapter (adapters/stm32/)
- Implements `loxboot_flash_adapter_t` for STM32 internal flash
- Uses STM32 HAL (FLASH_Program, FLASH_Erase) — only adapter file may use HAL
- Not tested in CTest (hardware-only)

### Step 8 — ESP32 flash adapter (adapters/esp32/)
- Implements `loxboot_flash_adapter_t` for ESP32 using esp_partition API
- Only adapter file may use ESP-IDF
- Not tested in CTest (hardware-only)

---

## Test framework

No external test framework. Tests use the same pattern as loxruntime:

```c
#include <stdio.h>
#include <assert.h>
#include "loxboot/loxboot.h"

static int passed = 0;
static int failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name)  do { test_##name(); } while(0)
#define CHECK(expr) do { \
    if (expr) { passed++; } \
    else { fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); failed++; } \
} while(0)

int main(void) {
    RUN(your_test_name);
    printf("passed=%d failed=%d\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
```

Every test file must:
- Use a **RAM flash adapter** (static buffer, no real flash)
- Use a **mock HAL** (captures on_fatal calls, counts jumps)
- Be fully deterministic
- Return exit code 1 if any CHECK fails

---

## RAM flash adapter (for tests)

Every test that touches flash uses this pattern:

```c
#define TEST_FLASH_SIZE (64 * 1024)
static uint8_t g_flash[TEST_FLASH_SIZE];

static loxboot_err_t ram_flash_read(void *ctx, uint32_t addr, uint8_t *buf, size_t len) {
    uint8_t *flash = (uint8_t *)ctx;
    if (addr + len > TEST_FLASH_SIZE) return LOXBOOT_ERR_FLASH_READ;
    memcpy(buf, flash + addr, len);
    return LOXBOOT_OK;
}

static loxboot_err_t ram_flash_write(void *ctx, uint32_t addr, const uint8_t *buf, size_t len) {
    uint8_t *flash = (uint8_t *)ctx;
    if (addr + len > TEST_FLASH_SIZE) return LOXBOOT_ERR_FLASH_WRITE;
    memcpy(flash + addr, buf, len);
    return LOXBOOT_OK;
}

static loxboot_err_t ram_flash_erase(void *ctx, uint32_t addr, size_t len) {
    uint8_t *flash = (uint8_t *)ctx;
    if (addr + len > TEST_FLASH_SIZE) return LOXBOOT_ERR_FLASH_ERASE;
    memset(flash + addr, 0xFF, len);
    return LOXBOOT_OK;
}
```

---

## Mock HAL (for tests)

```c
static int g_fatal_count = 0;
static loxboot_err_t g_fatal_reason = LOXBOOT_OK;
static int g_jump_count = 0;
static uint32_t g_jump_addr = 0;

static void mock_fatal(void *ctx, loxboot_err_t reason) {
    (void)ctx;
    g_fatal_count++;
    g_fatal_reason = reason;
    /* Do not actually halt in tests */
}

/* Jump hook — injected via loxboot_set_jump_hook() in test builds only */
static void mock_jump(void *ctx, uint32_t app_addr) {
    (void)ctx;
    g_jump_count++;
    g_jump_addr = app_addr;
}
```

`loxboot_run()` must check for a test jump hook before executing the real jump.
This is enabled only when `LOXBOOT_TEST_HOOKS=1` is defined at compile time.

---

## CMake build matrix

The CMakeLists.txt must support:

| Option | Default | Meaning |
|---|---|---|
| `LOXBOOT_BUILD_TESTS` | ON | Build and register CTest tests |
| `LOXBOOT_BUILD_UART_PORT` | OFF | Include ports/uart/ in build |
| `LOXBOOT_BUILD_STM32_ADAPTER` | OFF | Include adapters/stm32/ |
| `LOXBOOT_BUILD_ESP32_ADAPTER` | OFF | Include adapters/esp32/ |
| `LOXBOOT_MAX_BOOT_ATTEMPTS` | 3 | Compile-time rollback threshold |
| `LOXBOOT_TEST_HOOKS` | OFF | Enable test jump/fatal intercept hooks |

When `LOXBOOT_BUILD_TESTS=ON`, also define `LOXBOOT_TEST_HOOKS=1` automatically.

CI must run:
- Ubuntu GCC (core + tests)
- Ubuntu Clang (core + tests)
- Windows MSVC (core + tests)
- Windows ClangCL (core + tests)

---

## GitHub Actions (CI)

File: `.github/workflows/ci.yml`

Matrix:
```yaml
strategy:
  matrix:
    include:
      - os: ubuntu-latest
        compiler: gcc
      - os: ubuntu-latest
        compiler: clang
      - os: windows-latest
        compiler: msvc
      - os: windows-latest
        compiler: clang-cl
```

Steps per job:
1. cmake configure with `-DLOXBOOT_BUILD_TESTS=ON`
2. cmake build
3. ctest --output-on-failure

---

## Versioning policy

Same as loxruntime. Each milestone gets a tag:

| Tag | Milestone |
|---|---|
| `v0.1.0-spec` | API + spec baseline (docs only) |
| `v0.2.0-core` | CRC32 + init + state + slot control |
| `v0.3.0-boot-sequence` | loxboot_run + rollback + crash loop |
| `v0.4.0-uart` | UART transport port |
| `v0.5.0-stm32` | STM32 flash adapter |
| `v0.6.0-esp32` | ESP32 flash adapter |

Each tag must have:
- All tests passing in CI
- EVIDENCE_MATRIX.md updated
- PROJECT_STATE.md updated
- No unverified claims in EVIDENCE_MATRIX

---

## Commit discipline

- One logical change per commit
- Commit message format: `type: short description`
  - `feat:` new functionality
  - `test:` new or updated tests
  - `docs:` documentation only
  - `fix:` bug fix
  - `refactor:` no behavior change
  - `ci:` CI config
- Never commit code that breaks tests
- Never commit with warnings (treat warnings as errors in CI)

---

## Open questions to resolve before v0.2.0

These must be answered (by the human owner) before implementation begins:

1. **Boot state storage** — single copy or dual copy (primary + backup) with toggle?
   Recommendation: dual copy. Safer on power-loss during write.

2. **CRC32 polynomial** — standard 0xEDB88320 (ISO 3309) or configurable?
   Recommendation: standard, not configurable. Simplicity wins.

3. **loxboot_run() jump mechanism** — generic function pointer or Cortex-M specific (SCB->VTOR)?
   Recommendation: function pointer injected via platform config for v0.2.0.
   Cortex-M specific optimization deferred to v0.5.0 STM32 adapter.

4. **UART frame protocol** — custom or XMODEM/YMODEM compatible?
   Recommendation: custom (simpler, no royalty concerns, fits ecosystem style).

5. **Boot state region size** — fixed or configurable?
   Recommendation: fixed to `sizeof(loxboot_state_t)` per copy in v0.2.0-core.
