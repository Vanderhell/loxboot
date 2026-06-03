# loxboot — Claude Code prompty pre dokončenie projektu

Projekt je C99 bootloader core pre bare-metal MCU. **v0.2.0-core je hotový a overený.**
Nižšie sú prompty pre každý ďalší míľnik. Každý prompt sa spúšťa samostatne — vždy na čistom stave po commite predošlého míľnika.

---

## PROMPT 1 — v0.3.0-boot-sequence

```
Read these files in full before writing any code:
  AGENT_BRIEF.md
  docs/SPEC.md  (sections 6, 7, 8, 14)
  include/loxboot/loxboot.h
  src/loxboot_core.c
  src/loxboot_state.c
  tests/test_support.h
  tests/test_support.c
  CMakeLists.txt

You are implementing milestone v0.3.0-boot-sequence for loxboot.
All constraints from AGENT_BRIEF.md apply without exception.

--- What already exists ---
v0.2.0-core is complete and all CTests pass (MSVC + clang-cl).
loxboot_run() exists as a stub returning LOXBOOT_ERR_INVALID_STATE.

--- What you must implement ---

1. src/loxboot_core.c — implement loxboot_run() in full per SPEC.md §6:
   Step [1]: Read and validate boot state (dual-copy). Both corrupt → on_fatal(LOXBOOT_ERR_RECORD_CORRUPT).
   Step [2]: Determine candidate slot. No active slot → find VALID → promote to ACTIVE.
             No VALID slot → on_fatal(LOXBOOT_ERR_NO_VALID_SLOT).
   Step [3]: Check crash loop: slots[active].boot_attempts >= LOXBOOT_MAX_BOOT_ATTEMPTS → trigger rollback (§8).
   Step [4]: Increment boot_attempts on active slot. Write both copies. Failure → on_fatal(LOXBOOT_ERR_FLASH_WRITE).
   Step [5]: Verify firmware CRC32 over firmware_size bytes at slot base.
             Mismatch → mark slot INVALID, write state, retry from step [2].
   Step [6]: Promote PENDING → ACTIVE if selected slot was PENDING and CRC ok. Write state.
   Step [7]: Record boot_reason in state. Write state.
   Step [8]: Jump to application via loxboot_jump_to_app().
             In production: read SP from slot_base+0, entry from slot_base+4, apply Thumb bit, set MSR msp, call.
             When LOXBOOT_TEST_HOOKS=1: intercept via registered jump hook instead.

2. src/loxboot_core.c — add test hook API (compiled only when LOXBOOT_TEST_HOOKS=1):
   void loxboot_set_jump_hook(loxboot_jump_hook_t hook, void *ctx);
   The hook signature is: void (*)(void *ctx, uint32_t slot_base)
   loxboot_run() calls the hook instead of the real jump when it is registered.

3. Rollback procedure per SPEC.md §8:
   - Mark active slot LOXBOOT_SLOT_STATE_ROLLBACK
   - Find fallback: the other slot if state == VALID
   - If found: promote to ACTIVE, update active_slot, write state (both copies), set boot_reason = LOXBOOT_REASON_ROLLBACK, continue boot from step [3] with fallback
   - If not found: on_fatal(LOXBOOT_ERR_NO_VALID_SLOT)

4. src/loxboot_state.c — dual-copy corruption recovery:
   If one copy is corrupt and the other is valid: restore the corrupt copy from the valid one (write + verify).
   This runs transparently inside the existing state read path.

5. New test files — create all three from scratch:

   tests/test_loxboot_boot_sequence.c
   Tested scenarios (RAM flash adapter + jump hook + mock HAL — see AGENT_BRIEF.md):
   - Normal boot from VALID slot: jump hook called, boot_reason == LOXBOOT_REASON_NORMAL
   - PENDING slot verified ok: promoted to ACTIVE, jump hook called
   - PENDING slot CRC fail: slot marked INVALID, falls back to VALID slot
   - No valid slot at all: on_fatal called with LOXBOOT_ERR_NO_VALID_SLOT
   - Boot state both copies corrupt: on_fatal called with LOXBOOT_ERR_RECORD_CORRUPT
   - boot_attempts counter incremented before jump

   tests/test_loxboot_rollback.c
   Tested scenarios:
   - Crash loop detected (boot_attempts == LOXBOOT_MAX_BOOT_ATTEMPTS): rollback to VALID fallback, jump hook called, boot_reason == LOXBOOT_REASON_ROLLBACK
   - Rollback with no VALID fallback: on_fatal(LOXBOOT_ERR_NO_VALID_SLOT)
   - Active slot CRC fail triggers rollback to other slot

   tests/test_loxboot_crash_loop.c
   Tested scenarios:
   - Counter reaches threshold after N boots without loxboot_confirm_boot(): rollback triggered
   - loxboot_confirm_boot() resets counter → no rollback on next boot
   - Counter persists across simulated reboots (state written to RAM flash, re-read each run)

6. CMakeLists.txt:
   - Add all three new test executables under LOXBOOT_BUILD_TESTS.
   - LOXBOOT_TEST_HOOKS=1 must be defined automatically when LOXBOOT_BUILD_TESTS=ON (already in CMake, verify it still applies to new targets).

--- Constraints (non-negotiable) ---
- C99 only. No heap. No external includes beyond stdint.h/stddef.h/stdbool.h/string.h.
- Zero warnings on -Wall -Wextra -Wpedantic and MSVC /W4.
- loxboot_run() must never return in production (no return statement on the happy path).
- All tests fully deterministic, using only RAM flash adapter and mock HAL.
- Do not modify any public header. You may add static/internal functions to .c files.
- One commit per logical change: feat: / test: / fix: prefix.

--- Done when ---
cmake --build . && ctest --output-on-failure passes with zero failures for all targets including the three new ones.
Update EVIDENCE_MATRIX.md: mark all v0.3.0-boot-sequence rows as VERIFIED.
Update PROJECT_STATE.md: add v0.3.0-boot-sequence to completed baselines.
Tag: v0.3.0-boot-sequence
```

---

## PROMPT 2 — v0.4.0-uart

```
Read these files in full before writing any code:
  AGENT_BRIEF.md
  docs/SPEC.md  (section 11 in full — UART transport protocol)
  include/loxboot/loxboot.h
  include/loxboot/loxboot_transport.h
  src/loxboot_core.c
  tests/test_support.h
  tests/test_support.c
  CMakeLists.txt

You are implementing milestone v0.4.0-uart for loxboot.
All constraints from AGENT_BRIEF.md apply without exception.
v0.3.0-boot-sequence is complete and all CTests pass.

--- What you must implement ---

1. ports/uart/loxboot_uart.h  (new public header)
   Declare:
   - loxboot_uart_session_t  (opaque config struct — see SPEC.md §11)
   - loxboot_uart_run()      — runs the UART listen window, returns when done
   All types must use only types from loxboot.h and loxboot_transport.h.

2. ports/uart/loxboot_uart.c  (new implementation)

   a) CRC16-CCITT:
      static uint16_t loxboot_crc16(const uint8_t *data, size_t len)
      Polynomial: 0x1021, init value: 0xFFFF, table-driven, static const table.

   b) Frame encode:
      static loxboot_err_t frame_encode(uint8_t cmd, const uint8_t *payload, uint16_t len, uint8_t *out, size_t *out_len)
      Frame layout per SPEC.md §11: SOF(0x7E) | CMD(1B) | LEN_LO | LEN_HI | PAYLOAD(LEN B) | CRC16(2B LE)
      CRC16 covers: CMD + LEN_LO + LEN_HI + PAYLOAD.
      Max payload: LOXBOOT_UART_MAX_FRAME_PAYLOAD (default 1024).

   c) Frame decode:
      static loxboot_err_t frame_decode(const uint8_t *in, size_t in_len, uint8_t *cmd_out, uint8_t *payload_out, uint16_t *payload_len_out)
      Validate SOF, bounds, CRC16.
      CRC16 mismatch → return LOXBOOT_ERR_TRANSPORT.

   d) UART session — loxboot_uart_run():
      - Listen for CMD_HELLO for LOXBOOT_UART_LISTEN_MS ms using transport adapter read with timeout.
      - If no CMD_HELLO within window → return LOXBOOT_OK (boot normally).
      - On CMD_HELLO → send RSP_STATUS (slot_a_state, slot_b_state, active_slot, boot_reason).
      - Target slot = inactive slot (if active=A → target=B, else target=A; if unknown → Slot B).
      - Handle CMD_WRITE: validate offset+len <= slot_size, write to flash via flash adapter.
        Out of bounds → RSP_ERROR(LOXBOOT_ERR_INVALID_ARG).
        Flash error → RSP_ERROR(LOXBOOT_ERR_FLASH_WRITE), invalidate slot.
      - Handle CMD_COMMIT: verify CRC32 of written data against provided crc32, verify size.
        CRC match → mark slot PENDING, write boot state, send RSP_OK.
        CRC mismatch → RSP_ERROR(LOXBOOT_ERR_CRC_MISMATCH), mark slot INVALID.
      - Handle CMD_ABORT: invalidate target slot, send RSP_OK, return.
      - Handle CMD_STATUS: send RSP_STATUS.
      - Handle CMD_REBOOT: send RSP_OK, call hal.on_reboot() if provided, else spin.
      - Any CRC16 frame error → send RSP_ERROR(LOXBOOT_ERR_TRANSPORT), discard, continue listening.
      - Per-byte timeout (transport.timeout_ms): if exceeded mid-frame → invalidate slot, return LOXBOOT_ERR_TIMEOUT.

   e) Integrate loxboot_uart_run() into loxboot_run() in src/loxboot_core.c:
      When LOXBOOT_BUILD_UART_PORT=ON and a transport adapter is non-NULL in loxboot_platform_t,
      call loxboot_uart_run() before step [2] (slot selection).
      loxboot_run() is otherwise unchanged.

3. New test files — create both from scratch:

   tests/test_loxboot_uart_frame.c
   Test CRC16 known vectors, frame encode round-trip, frame decode with correct data,
   frame decode with corrupted CRC16 (→ LOXBOOT_ERR_TRANSPORT), empty payload, max payload.

   tests/test_loxboot_uart_receive.c
   Use an in-memory byte-buffer transport adapter (push/pop bytes, no real UART).
   Test scenarios:
   - No CMD_HELLO within listen window → function returns, boot state unchanged.
   - CMD_HELLO → RSP_STATUS sent correctly.
   - CMD_WRITE sequence → data written to RAM flash at correct offsets.
   - CMD_COMMIT with correct CRC32 → slot marked PENDING.
   - CMD_COMMIT with wrong CRC32 → RSP_ERROR, slot marked INVALID.
   - CMD_ABORT → slot invalidated, RSP_OK.
   - CMD_WRITE out of bounds → RSP_ERROR(LOXBOOT_ERR_INVALID_ARG).
   - Frame with corrupt CRC16 → RSP_ERROR(LOXBOOT_ERR_TRANSPORT), session continues.
   - Byte timeout mid-frame → session ends, slot invalidated.

4. CMakeLists.txt:
   - Add ports/uart/ to build when LOXBOOT_BUILD_UART_PORT=ON.
   - Add test executables for both new test files when both LOXBOOT_BUILD_TESTS=ON and LOXBOOT_BUILD_UART_PORT=ON.
   - CI matrix: add a separate job with -DLOXBOOT_BUILD_UART_PORT=ON.

--- Constraints (non-negotiable) ---
- C99 only. No heap. No external includes beyond stdint.h/stddef.h/stdbool.h/string.h.
- Zero warnings on -Wall -Wextra -Wpedantic and MSVC /W4.
- ports/uart/ may include loxboot.h and loxboot_transport.h only.
- No transport adapter changes — use the existing loxboot_transport_adapter_t from loxboot_transport.h.
- Do not modify existing public headers.
- All tests fully deterministic with in-memory adapters.

--- Done when ---
cmake --build . -DLOXBOOT_BUILD_UART_PORT=ON && ctest --output-on-failure passes with zero failures.
Update EVIDENCE_MATRIX.md: mark all v0.4.0-uart rows as VERIFIED.
Update PROJECT_STATE.md: add v0.4.0-uart to completed baselines.
Tag: v0.4.0-uart
```

---

## PROMPT 3 — v0.5.0-stm32

```
Read these files in full before writing any code:
  AGENT_BRIEF.md
  docs/PORTING.md
  include/loxboot/loxboot.h
  CMakeLists.txt

You are implementing milestone v0.5.0-stm32 for loxboot — the STM32 internal flash adapter.
All constraints from AGENT_BRIEF.md apply.
v0.4.0-uart is complete.

--- What you must implement ---

1. adapters/stm32/loxboot_flash_stm32.h  (new public header)
   Declare:
   - loxboot_stm32_flash_ctx_t  (holds nothing extra for now — flash is at fixed addresses)
   - loxboot_stm32_flash_adapter_init(loxboot_flash_adapter_t *out, loxboot_stm32_flash_ctx_t *ctx)
     Fills out->read/write/erase/ctx. Returns void.

2. adapters/stm32/loxboot_flash_stm32.c  (new implementation)
   Implement read, write, erase using STM32 HAL:
   - read:  memcpy from flash address (direct memory-mapped read on STM32).
   - write: HAL_FLASH_Unlock() → HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ...) per 8 bytes → HAL_FLASH_Lock().
            Return LOXBOOT_ERR_FLASH_WRITE on HAL error.
   - erase: HAL_FLASH_Unlock() → FLASH_EraseInitTypeDef + HAL_FLASHEx_Erase() → HAL_FLASH_Lock().
            Erase is page-aligned; if addr or len is not page-aligned, return LOXBOOT_ERR_INVALID_ARG.
            Return LOXBOOT_ERR_FLASH_ERASE on HAL error.
   Only this file may include STM32 HAL headers (stm32XXxx_hal_flash.h etc).
   The adapter must compile cleanly with any STM32 HAL variant by using the generic HAL header:
     #include "stm32_hal.h"  (user provides this via include path — document in PORTING.md).

3. CMakeLists.txt:
   - Add adapters/stm32/ to build when LOXBOOT_BUILD_STM32_ADAPTER=ON.
   - STM32 adapter is hardware-only — do NOT add CTest entries for it.
   - CI must NOT set LOXBOOT_BUILD_STM32_ADAPTER=ON (no STM32 toolchain in CI).

4. docs/PORTING.md:
   Update the STM32 section: explain how to set the include path for stm32_hal.h,
   link the adapter, and call loxboot_stm32_flash_adapter_init() from board init code.

--- Constraints ---
- C99 only. No heap. adapters/stm32/ may include loxboot.h and the STM32 HAL, nothing else.
- The HAL header is abstracted through a single #include "stm32_hal.h" — document this.
- No CTest entries (hardware-only verification).
- Zero warnings when compiled with arm-none-eabi-gcc -Wall -Wextra -Wpedantic (user verifies on hardware).

--- Done when ---
cmake --build . -DLOXBOOT_BUILD_STM32_ADAPTER=ON compiles without errors (STM32 HAL headers provided via -I).
Update EVIDENCE_MATRIX.md: mark STM32 adapter row as IMPLEMENTED (not VERIFIED — hardware required).
Update PROJECT_STATE.md.
Tag: v0.5.0-stm32
```

---

## PROMPT 4 — v0.6.0-esp32

```
Read these files in full before writing any code:
  AGENT_BRIEF.md
  docs/PORTING.md
  include/loxboot/loxboot.h
  adapters/stm32/loxboot_flash_stm32.h   (use as structural reference)
  CMakeLists.txt

You are implementing milestone v0.6.0-esp32 — the ESP32 flash adapter using esp_partition API.
All constraints from AGENT_BRIEF.md apply.
v0.5.0-stm32 is complete.

--- What you must implement ---

1. adapters/esp32/loxboot_flash_esp32.h  (new public header)
   Declare:
   - loxboot_esp32_flash_ctx_t  { const esp_partition_t *partition; }
   - loxboot_esp32_flash_adapter_init(loxboot_flash_adapter_t *out, loxboot_esp32_flash_ctx_t *ctx)

2. adapters/esp32/loxboot_flash_esp32.c  (new implementation)
   Implement read, write, erase using ESP-IDF esp_partition API:
   - read:  esp_partition_read(ctx->partition, addr, buf, len). Return LOXBOOT_ERR_FLASH_READ on ESP_OK failure.
   - write: esp_partition_write(ctx->partition, addr, buf, len). Return LOXBOOT_ERR_FLASH_WRITE on failure.
   - erase: esp_partition_erase_range(ctx->partition, addr, len). Return LOXBOOT_ERR_FLASH_ERASE on failure.
   Only this file may include ESP-IDF headers (esp_partition.h).

3. CMakeLists.txt:
   - Add adapters/esp32/ to build when LOXBOOT_BUILD_ESP32_ADAPTER=ON.
   - No CTest entries (hardware-only).
   - CI must NOT set LOXBOOT_BUILD_ESP32_ADAPTER=ON.

4. docs/PORTING.md:
   Update the ESP32 section: explain how to find the target partition by name
   (esp_partition_find_first()), initialize the context, and call loxboot_run()
   from app_main() before the main application task.

--- Constraints ---
- C99 only. No heap. adapters/esp32/ may include loxboot.h and ESP-IDF headers, nothing else.
- No CTest entries.
- Zero warnings with xtensa-esp32-elf-gcc -Wall -Wextra (user verifies on hardware).

--- Done when ---
cmake --build . -DLOXBOOT_BUILD_ESP32_ADAPTER=ON compiles (ESP-IDF headers provided via -I).
Update EVIDENCE_MATRIX.md: mark ESP32 adapter row as IMPLEMENTED.
Update PROJECT_STATE.md.
Tag: v0.6.0-esp32
```

---

## PROMPT 5 — CI: GitHub Actions (spustiť po v0.3.0)

```
Read these files in full before making any changes:
  AGENT_BRIEF.md
  CMakeLists.txt
  .github/workflows/ci.yml  (if it exists)

You are fixing and verifying the GitHub Actions CI for loxboot.
v0.3.0-boot-sequence is complete. All CTests must pass on all four matrix targets.

--- What you must do ---

1. Verify .github/workflows/ci.yml has the full 4-target matrix:
   - ubuntu-latest / gcc
   - ubuntu-latest / clang
   - windows-latest / msvc
   - windows-latest / clang-cl

2. Each job must:
   a. cmake -B build -DLOXBOOT_BUILD_TESTS=ON  (+ appropriate generator and toolchain flags)
   b. cmake --build build --config Release
   c. ctest --test-dir build --build-config Release --output-on-failure

3. For ubuntu/gcc job add -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic -Werror"
   For ubuntu/clang job add -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic -Werror"
   For MSVC job add -DCMAKE_C_FLAGS="/W4 /WX"
   For clang-cl job add -DCMAKE_C_FLAGS="-Wall -Wextra -Werror"

4. If any job currently fails on Linux (GCC/Clang warnings treated as errors):
   Fix the warnings in src/ and include/ — do NOT suppress warnings with pragmas.
   Common issues: signed/unsigned comparison, unused parameters (cast to void), missing-field-initializers.

5. After all four jobs pass:
   Update EVIDENCE_MATRIX.md: mark "GitHub Actions CI passing" row as VERIFIED.
   Update PROJECT_STATE.md note about Linux GCC/Clang verification.

--- Done when ---
All four GitHub Actions jobs are green on the main branch.
```

---

## Poradie spustenia promptov

| # | Prompt | Vetva / tag |
|---|--------|-------------|
| 1 | v0.3.0-boot-sequence | `v0.3.0-boot-sequence` |
| 2 | CI fix (po 0.3.0) | — |
| 3 | v0.4.0-uart | `v0.4.0-uart` |
| 4 | v0.5.0-stm32 | `v0.5.0-stm32` |
| 5 | v0.6.0-esp32 | `v0.6.0-esp32` |

Každý prompt spúšťaj až keď je predošlý míľnik otag­ovaný a CI zelené.
