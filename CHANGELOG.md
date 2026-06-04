# Changelog

All notable changes to loxboot are documented in this file.

## [v0.6.0] — 2026-06-04

### Critical Bug Fixes
- **UART frame decode:** Fixed zero-payload minimum size check (6 bytes, not 7)
  - Impact: CMD_HELLO, CMD_ABORT, CMD_REBOOT now decode correctly
- **Session state gating:** WRITE/COMMIT/REBOOT now require HELLO first
  - Impact: Prevents unauthorized commands
- **COMMIT validation:** Firmware size must equal bytes written
  - Impact: Catches upload truncation/mismatch before commit
- **Frame API NULL checks:** All encode/decode functions validate input pointers
  - Impact: Prevents NULL dereference in UART path
- **Slot erase timing:** Moved from session init to first WRITE
  - Impact: Prevents fallback firmware destruction during passive boot
- **CMake UART integration:** Added LOXBOOT_BUILD_UART_PORT compile definition
  - Impact: UART session code now executes when flag is ON
- **Adapter header includes:** Fixed incorrect #include paths in adapter stubs
  - Impact: Build no longer fails on missing headers
- **Uninitialized variable:** Initialize huge_payload array in tests
  - Impact: Eliminates GCC warning on uninitialized local array

### Features
- **Incremental CRC32 API:** Added init/update/finalize for streaming verification
- **Full UART session test:** Added test covering HELLO→WRITE→COMMIT→STATUS→REBOOT flow

### Improvements
- **Documentation:** Added RELEASE_CHECKLIST.md, EVIDENCE_MATRIX.md, docs/PROTOCOL_UART.md, docs/PLATFORM_STATUS.md
- **Build verification:** Verified GCC/Clang warning flags configured in CMake
- **Test expansion:** 336 → 362 tests with new frame/session validation cases

### Test Results
```
Boot sequence:         17 tests ✅
State management:     132 tests ✅
UART frame:            43 tests ✅
UART session:          34 tests ✅
Slot operations:       25 tests ✅
Init/CRC/rollback:     37 tests ✅
Misc:                  74 tests ✅
────────────────────────────────
Total:                362 tests ✅ (100% pass rate)
```

---

## [v0.5.0-stm32] — STM32 internal flash adapter
- HAL-based flash operations (read/write/erase)

## [v0.4.0-uart] — UART transport protocol
- Frame format and CRC16-CCITT
- Commands: HELLO, WRITE, COMMIT, ABORT, STATUS, REBOOT

## [v0.3.0-boot-sequence] — Full boot sequence
- 8-step boot sequence with crash loop detection

## [v0.2.0-core] — Core bootloader engine
- CRC32, state management, slot control

## [v0.1.0-spec] — API specification
- Public API and platform adapter interfaces
