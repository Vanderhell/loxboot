# v0.7.0 Status

## Code Quality ✅
- [x] 15/15 CTest binaries passing in this workspace
- [x] MSVC Debug build passed under the existing `/W4 /WX` settings
- [x] GCC/Clang flags configured (`-Wall -Wextra -Wpedantic -Werror`, `CMakeLists.txt`)
- [x] C99 only, no external dependencies
- [x] 8 critical bugs fixed: frame validation, session gating, erase timing, NULL checks, adapter headers, uninitialized variables

## UART Protocol ✅
- [x] Frame encode/decode with CRC16-CCITT
- [x] Session gating (HELLO required before WRITE/COMMIT/REBOOT)
- [x] COMMIT validation (firmware_size == _bytes_written)
- [x] Slot erase on first WRITE (not on session init)
- [x] NULL pointer validation on all public APIs
- [x] Error propagation (write_byte, flush failures)

## Firmware Verification ✅
- [x] Incremental CRC32 API (init/update/finalize)
- [x] Firmware CRC verified via flash.read() (not direct memory)
- [x] CRC verification works for non-memory-mapped flash

## Boot Sequence ✅
- [x] 8-step loxboot_run() boot sequence
- [x] Crash loop detection and automatic rollback
- [x] Dual-copy state read/write/validate with corruption recovery
- [x] All slot state transitions tested

## Test Coverage ✅ (local verification)
- [x] 15/15 CTest binaries passing in this workspace
- [x] Boot sequence, state management, UART frame/session, slot operations, init/CRC/rollback
- [x] ESP32 platform stub tests
- [ ] Hardware adapter validation: not verified locally (requires real HAL/IDF)

## Build Status ✅
- [x] MSVC: build and CTest passed locally in this workspace
- [ ] GCC: NOT RUN / unavailable locally
- [ ] Clang: NOT RUN / unavailable locally
- [x] LOXBOOT_BUILD_UART_PORT properly wired in CMake

## Documentation ✅
- [x] README: NOT PRODUCTION READY, local verification count consistent
- [x] API headers: frame_encode/decode, loxboot_format_state() in public API
- [x] CHANGELOG.md: v0.7.0 entry complete
- [x] KNOWN_ISSUES.md: All blockers documented
- [x] docs/PROTOCOL_UART.md: CRC16 and erase timing corrected
- [x] docs/PLATFORM_STATUS.md: Per-platform guidance
- [x] docs/TEST_PLAN.md: E2E test status updated

## New in v0.7.0 ✅
- [x] loxboot_format_state() public provisioning API
- [x] Fresh flash auto-recovery in loxboot_run()
- [x] E2E simulator (tools/loxboot_sim.c) in CTest
- [x] Python E2E test suite runs in CTest
- [ ] Hardware E2E on ESP32-S3: NOT VERIFIED locally
- [x] ESP32-S3 IDF project (idf_project/)
- [x] Release workflow with Windows MSVC .lib artifact
- [x] Usage examples (examples/)
- [x] STM32 + ESP32 erase granularity both fixed

## Known Limitations ⚠️
- Slot erase granularity: Core assumes flash can erase arbitrary sizes
  (STM32/ESP32 adapters may need to round up to sector boundaries)
- Boot state write: 52 bytes, may trigger platform erase granularity issues
- Full update flow test: Validates command sequence, not CRC accuracy
- Adapter tests: Require real hardware (STM32 HAL, ESP32 IDF)

## Hardware Adapters (Code complete, not tested)
- [ ] STM32 adapter: Code complete, needs real STM32 + HAL validation
- [ ] ESP32 adapter: Code complete, needs real ESP32 + IDF validation
- [ ] Jump mechanism: ARM Cortex-M implementation, needs hardware test

## What Works ✅
- Boot sequence on any platform
- UART protocol implementation
- Frame-level CRC validation
- Session state machine
- All documented in code and tests

## What Requires Hardware ❌
- Actual flash erase/write behavior
- Jump to application code
- UART serial transmission (tested via mock transport)
- Power-loss recovery scenarios
- Real adapter integration

## Status Summary

**This repository remains NOT PRODUCTION READY.**

Not a release candidate (hardware validation required), but:
- Local automated tests pass in this workspace (15/15 CTest binaries)
- All identified bugs fixed
- Code is clean and compilable
- Protocol is documented
- GitHub Actions: NOT RUN / unavailable locally
- Hardware validation remains incomplete.

**Next step:** Validate on real STM32 or ESP32 with appropriate HAL/IDF.
