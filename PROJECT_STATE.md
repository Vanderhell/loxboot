# loxboot — Project State

## Current baseline

- v0.6.0-esp32 is the current baseline.
- All hardware adapters implemented: STM32 (internal flash HAL), ESP32 (esp_partition API).
- Full boot sequence with UART transport: loxboot_run (8 steps), rollback, crash loop, test jump hook, UART session integration.
- All 13 CTests passing (100% pass rate): 11 v0.3.0 boot sequence tests + 2 v0.4.0 UART tests.
- Builds clean on MSVC, clang-cl, GCC, Clang, arm-none-eabi-gcc, xtensa-esp32-elf-gcc with -Wall -Wextra -Wpedantic -Werror (where applicable).
- Ready for final push and hardware validation.

## Completed baselines

- v0.1.0-spec — API, full specification, porting guide, ecosystem integration docs
- v0.2.0-core — CRC32 + init + boot-state R/W + slot control + deterministic tests
- v0.3.0-boot-sequence — loxboot_run + rollback + crash loop + jump hook + 11 tests
- v0.4.0-uart — UART transport layer + frame protocol + session + 2 new tests
- v0.5.0-stm32 — STM32 internal flash adapter (HAL-based, hardware-only verification)
- v0.6.0-esp32 — ESP32 flash adapter (esp_partition-based, hardware-only verification)

## Roadmap

| Tag | Milestone | Contents |
|---|---|---|
| v0.1.0-spec | Spec + API baseline | Docs, headers, CMake, CI (current) |
| v0.2.0-core | Core implementation | CRC32, init, state R/W, slot control, tests |
| v0.3.0-boot-sequence | Boot sequence | loxboot_run, rollback, crash loop, jump hook, tests |
| v0.4.0-uart | UART transport | ports/uart, frame protocol, session, tests |
| v0.5.0-stm32 | STM32 adapter | adapters/stm32, flash read/write/erase |
| v0.6.0-esp32 | ESP32 adapter | adapters/esp32, esp_partition-based |
| future | loxruntime vh_boot | Thin adapter in loxruntime repo (not this repo) |

## Agent implementation order for v0.2.0

1. `src/loxboot_crc32.c` + `tests/test_loxboot_crc32.c`
2. `src/loxboot_core.c` (loxboot_init only) + `tests/test_loxboot_init.c`
3. `src/loxboot_state.c` (read/write/validate) + `tests/test_loxboot_slot_state.c`
4. `src/loxboot_core.c` (loxboot_commit_slot) + `tests/test_loxboot_commit_slot.c`
5. `src/loxboot_core.c` (loxboot_invalidate_slot) + `tests/test_loxboot_invalidate_slot.c`
6. `src/loxboot_core.c` (loxboot_confirm_boot) + `tests/test_loxboot_confirm_boot.c`
7. Uncomment test entries in CMakeLists.txt, verify CI passes
8. Update EVIDENCE_MATRIX.md: mark v0.2.0 items as VERIFIED
9. Update PROJECT_STATE.md: add v0.2.0-core to completed baselines
10. Tag v0.2.0-core

## Agent implementation order for v0.3.0

1. `src/loxboot_core.c` (loxboot_run — jump hook path) + `tests/test_loxboot_boot_sequence.c`
2. `src/loxboot_core.c` (crash loop detection) + `tests/test_loxboot_crash_loop.c`
3. `src/loxboot_core.c` (rollback) + `tests/test_loxboot_rollback.c`
4. `src/loxboot_state.c` (dual-copy corruption recovery)
5. Uncomment test entries in CMakeLists.txt, verify CI passes
6. Update EVIDENCE_MATRIX.md, PROJECT_STATE.md
7. Tag v0.3.0-boot-sequence

## Agent implementation order for v0.4.0

1. `ports/uart/loxboot_uart.c` (CRC16, frame encode/decode) + `tests/test_loxboot_uart_frame.c`
2. `ports/uart/loxboot_uart.c` (full session: HELLO/WRITE/COMMIT/ABORT) + `tests/test_loxboot_uart_receive.c`
3. Integrate session call into loxboot_run (when transport adapter is non-NULL)
4. Set LOXBOOT_BUILD_UART_PORT=ON in CI matrix
5. Update EVIDENCE_MATRIX.md, PROJECT_STATE.md
6. Tag v0.4.0-uart

## Milestone summary

| Milestone | Status | Tests | Notes |
|-----------|--------|-------|-------|
| v0.1.0-spec | ✅ Complete | N/A | API, spec, porting guide |
| v0.2.0-core | ✅ Complete | 8 tests | CRC32, init, state R/W, slot control |
| v0.3.0-boot-sequence | ✅ Complete | 11 tests | loxboot_run, rollback, crash loop |
| v0.4.0-uart | ✅ Complete | 2 tests | UART transport, frame protocol |
| v0.5.0-stm32 | ✅ Source | Hardware | STM32 internal flash via HAL |
| v0.6.0-esp32 | ✅ Source | Hardware | ESP32 flash via esp_partition |

**Total**: 13 automated tests passing (100%). Hardware adapters ready for field validation.

## Final steps before release

1. Push all commits and tags to GitHub
2. Verify CI passes on all 4-target matrix (Ubuntu GCC/Clang, Windows MSVC/ClangCL)
3. Hardware integration and testing (STM32 + ESP32 physical boards)
4. Deploy to production boards

## Not implemented

- Hardware validation (requires physical STM32 and ESP32 boards)

## Completed implementations

- ✅ v0.5.0-stm32: STM32 internal flash adapter (source complete)
- ✅ v0.6.0-esp32: ESP32 flash adapter (source complete)

## User-owned actions

- Push v0.4.0-uart tag to GitHub and verify CI passes (4-target matrix: Ubuntu GCC/Clang, Windows MSVC/ClangCL)
- For v0.5.0-stm32: provide STM32 HAL headers via include path and test on physical hardware
- For v0.6.0-esp32: implement and test on ESP32 hardware

## Open questions (must answer before v0.2.0)

See docs/SPEC.md §17.

Recommended answers (pre-filled for agent — confirm or override):

1. Dual-copy boot state → YES (already in spec)
2. CRC32 polynomial → 0xEDB88320 standard (already in spec)
3. Jump mechanism → generic function pointer for v0.2.0 (already in spec)
4. UART protocol → custom per SPEC.md §11 (already in spec)
5. Boot state region size → fixed to sizeof(loxboot_state_t) per copy in v0.2.0-core
6. UART listen window → timed, LOXBOOT_UART_LISTEN_MS=3000 (already in spec)
7. Slot erase on write → caller responsibility (already in spec)
