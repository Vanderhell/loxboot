# loxboot — Project State

## Current baseline

- v0.4.0-uart is the current baseline.
- Full boot sequence with UART transport: loxboot_run (8 steps), rollback, crash loop, test jump hook, UART session integration.
- All 13 CTests passing (100% pass rate): 11 v0.3.0 boot sequence tests + 2 v0.4.0 UART tests.
- Builds clean on MSVC, clang-cl, GCC, Clang with -Wall -Wextra -Wpedantic -Werror.
- Ready for GitHub push and CI verification.

## Completed baselines

- v0.1.0-spec — API, full specification, porting guide, ecosystem integration docs
- v0.2.0-core — CRC32 + init + boot-state R/W + slot control + deterministic tests
- v0.3.0-boot-sequence — loxboot_run + rollback + crash loop + jump hook + 11 tests
- v0.4.0-uart — UART transport layer + frame protocol + session + 2 new tests

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

## Next steps

1. ✅ v0.4.0-uart complete (all 13 tests passing, source committed)
2. ⏳ v0.5.0-stm32 source code written (awaiting hardware for validation)
3. 🔄 v0.6.0-esp32 to follow (v0.5.0-stm32 tested)

## Not implemented

- Hardware adapters (STM32/ESP32 v0.5.0–v0.6.0) — source code written, hardware validation pending
- Hardware validation (requires physical boards)

## In progress

- v0.5.0-stm32: STM32 internal flash adapter (source complete, awaiting hardware test)
- v0.6.0-esp32: ESP32 flash adapter (next milestone)

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
