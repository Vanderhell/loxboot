# loxboot — Bootloader Core

Minimal, adapter-based bootloader core for bare-metal MCUs. Production-ready for **ARM Cortex-M** (STM32, others). Partial support for Xtensa and RISC-V (adapters implemented; jump mechanism ARM-only).

**Latest release:** v0.6.0-esp32

---

## Overview

loxboot is a **C99, zero-heap bootloader core** that handles:

- **Boot state management:** Dual-copy read/write with CRC32 corruption recovery
- **Slot control:** Firmware A/B with metadata (commit, invalidate, request, confirm)
- **Boot sequence:** Full 8-step startup with crash loop detection and automatic rollback
- **UART transport:** In-field firmware updates via frame protocol (v0.4.0+)
- **Hardware adapters:** STM32 internal flash (v0.5.0) and ESP32 esp_partition (v0.6.0)

All hardware access is injected via adapter function pointers — no vendor-specific code in core.

---

## Architecture

### Core (src/)
- `loxboot_core.c` — loxboot_run(), slot control, state management
- `loxboot_state.c` — State read/write/validate with dual-copy recovery
- `loxboot_crc32.c` — CRC32-CCITT implementation (polynomial 0xEDB88320)

### Adapters (adapters/)
- `adapters/stm32/loxboot_flash_stm32.c` — STM32 HAL flash driver
- `adapters/esp32/loxboot_flash_esp32.c` — ESP32 esp_partition driver

### Transport (ports/)
- `ports/uart/loxboot_uart.c` — UART frame protocol (CRC16-CCITT, SOF | CMD | LEN | PAYLOAD | CRC16)

### Headers (include/loxboot/)
- `loxboot.h` — Public API (boot state, slot control, error codes)
- `loxboot_transport.h` — Transport adapter interface
- `loxboot_version.h` — Version constants

---

## Quick Start

```c
#include "loxboot/loxboot.h"

/* 1. Initialize adapters */
loxboot_t ctx = {0};

ctx.flash.read  = my_flash_read;
ctx.flash.write = my_flash_write;
ctx.flash.erase = my_flash_erase;
ctx.clock.now_ms = my_clock_now_ms;
ctx.hal.on_fatal = my_fatal;

/* 2. Set platform addresses */
ctx.platform.boot_state_primary_base = 0x08004000;
ctx.platform.boot_state_backup_base  = 0x08008000;
ctx.platform.slot_a_base             = 0x08020000;
ctx.platform.slot_b_base             = 0x08060000;
ctx.platform.slot_size               = 0x00040000;

/* 3. Initialize state */
if (loxboot_init(&ctx) != LOXBOOT_OK) {
    ctx.hal.on_fatal(&ctx, LOXBOOT_ERR_INVALID_STATE);
}

/* 4. Run bootloader (never returns on success) */
loxboot_run(&ctx);
```

In the application, after startup validation:
```c
loxboot_confirm_boot(&ctx);  /* Resets crash counter */
```

---

## Feature Matrix

| Feature | v0.1.0 | v0.2.0 | v0.3.0 | v0.4.0 | v0.5.0 | v0.6.0 |
|---------|--------|--------|--------|--------|--------|--------|
| Public API + spec | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| CRC32, init, slot control | spec | ✅ | ✅ | ✅ | ✅ | ✅ |
| Boot sequence + rollback + crash detection | spec | — | ✅ | ✅ | ✅ | ✅ |
| UART transport (CRC16, frame protocol) | spec | — | — | ✅ | ✅ | ✅ |
| STM32 flash adapter (HAL-based) | — | — | — | — | ✅ | ✅ |
| ESP32 flash adapter (esp_partition) | — | — | — | — | — | ✅ |

---

## Build

### Configure
```bash
cmake -S . -B build -DLOXBOOT_BUILD_TESTS=ON
```

Optional flags:
```bash
-DLOXBOOT_BUILD_UART_PORT=ON          # Include UART transport
-DLOXBOOT_BUILD_STM32_ADAPTER=ON      # Include STM32 flash adapter
-DLOXBOOT_BUILD_ESP32_ADAPTER=ON      # Include ESP32 flash adapter
-DLOXBOOT_MAX_BOOT_ATTEMPTS=3         # Rollback threshold (default: 3)
-DLOXBOOT_UART_LISTEN_MS=3000         # UART listen window ms (default: 3000)
```

### Build & Test
```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

### Compiler Requirements
- C99 standard
- No heap (no malloc, calloc, realloc, free)
- No external dependencies (only stdint.h, stddef.h, stdbool.h, string.h)
- Zero warnings: `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang) or `/W4 /WX` (MSVC)

---

## Documentation

| Document | Purpose |
|----------|---------|
| [docs/SPEC.md](docs/SPEC.md) | Full technical specification (all versions) |
| [docs/PORTING.md](docs/PORTING.md) | How to write a flash or transport adapter |
| [docs/MEMORY_LAYOUT.md](docs/MEMORY_LAYOUT.md) | Flash and RAM layout reference |
| [docs/INTEGRATION.md](docs/INTEGRATION.md) | Ecosystem integration (loxruntime, panicdump, nvlog) |

---

## Test Coverage

**13 automated tests (100% passing):**

- **v0.2.0 core (8 tests):** CRC32, init, state R/W, slot control
- **v0.3.0 boot sequence (3 tests):** loxboot_run, rollback, crash loop
- **v0.4.0 UART (2 tests):** Frame protocol, session handling

**Tested in CI (4-target matrix):**
- Ubuntu GCC with `-Wall -Wextra -Wpedantic -Werror`
- Ubuntu Clang with `-Wall -Wextra -Wpedantic -Werror`
- Windows MSVC with `/W4 /WX`
- Windows Clang-cl with `-Wall -Wextra -Werror`

**Note:** CI tests core + UART (13 tests). Hardware adapters (STM32, ESP32) are **not** tested in CI because they require vendor HAL headers (stm32_hal.h, esp_partition.h). Each adapter must be verified on physical hardware with the target platform's HAL/IDF.

---

## API Reference

### Boot Sequence
```c
loxboot_err_t loxboot_init(loxboot_t *ctx);
loxboot_err_t loxboot_run(loxboot_t *ctx);  /* Never returns on success */
```

### Slot Control
```c
loxboot_err_t loxboot_commit_slot(loxboot_t *ctx, loxboot_slot_id_t slot, uint32_t firmware_size, uint32_t firmware_crc32);
loxboot_err_t loxboot_invalidate_slot(loxboot_t *ctx, loxboot_slot_id_t slot);
loxboot_err_t loxboot_request_slot(loxboot_t *ctx, loxboot_slot_id_t slot);
loxboot_err_t loxboot_confirm_boot(loxboot_t *ctx);
```

### State Access
```c
loxboot_err_t loxboot_state_read(loxboot_t *ctx, loxboot_state_t *out_state);
loxboot_err_t loxboot_get_slot_state(loxboot_t *ctx, loxboot_slot_id_t slot, loxboot_slot_state_t *out_state);
loxboot_boot_reason_t loxboot_get_boot_reason(const loxboot_t *ctx);
```

### Error Codes
```c
LOXBOOT_OK                  = 0
LOXBOOT_ERR_INVALID_ARG     = 1
LOXBOOT_ERR_FLASH_READ      = 2
LOXBOOT_ERR_FLASH_WRITE     = 3
LOXBOOT_ERR_FLASH_ERASE     = 4
LOXBOOT_ERR_CRC_MISMATCH    = 5
LOXBOOT_ERR_NO_VALID_SLOT   = 6
LOXBOOT_ERR_TIMEOUT         = 7
LOXBOOT_ERR_TRANSPORT       = 8
LOXBOOT_ERR_INVALID_STATE   = 9
LOXBOOT_ERR_RECORD_CORRUPT  = 10
```

---

## Configuration

### Boot State Region
Minimum 2 copies of `loxboot_state_t` (typically ~60 bytes each):
```c
boot_state_primary_base  = 0x08004000
boot_state_backup_base   = 0x08008000
```

On flash with large sector sizes, allocate one sector per copy.

### Firmware Slots
```c
slot_a_base = 0x08020000  (Slot A)
slot_b_base = 0x08060000  (Slot B)
slot_size   = 0x00040000  (256 KB each)
```

Minimum 4 KB per slot (practical minimum: 32 KB for typical firmware).

### Boot Attempts Threshold
Default: `LOXBOOT_MAX_BOOT_ATTEMPTS = 3`

If firmware fails to call `loxboot_confirm_boot()` within 3 boots, automatic rollback triggers.

### UART Listen Window
Default: `LOXBOOT_UART_LISTEN_MS = 3000` (3 seconds)

After this timeout with no UART activity, boot proceeds normally.

---

## Constraints

- **C99 only** — no C11+ features (no atomics, no threads)
- **Zero heap** — all data structures are stack-allocated or static
- **No external dependencies** — bootloader is self-contained
- **Adapter injection** — all hardware access via function pointers
- **Deterministic** — no floating-point, no randomness (except UART timing)
- **Synchronous** — all operations block until complete (no async model)

---

## License

MIT
