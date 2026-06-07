# Changelog

All notable changes to loxboot are documented in this file.

## [v0.7.0] - 2026-06-04

### New Features
- `loxboot_format_state()` public API for factory provisioning.
- Fresh flash auto-recovery in `loxboot_run()`.
- E2E simulator (`tools/loxboot_sim.c`) for the UART protocol.
- Python E2E test suite (`tools/test_e2e.py`) wired into CTest.
- ESP32-S3 IDF project (`idf_project/`) with USB Serial JTAG transport.
- Usage examples (`examples/`) for STM32, ESP32, and bare-metal adapters.

### Bug Fixes
- UART frame decode: minimum frame size corrected from 7 to 6 bytes.
- Session gating: WRITE/COMMIT/REBOOT now require HELLO first.
- COMMIT size validation: `firmware_size` must equal bytes written.
- `loxboot_uart_run_session()`: NULL `flush` callback rejected.
- STM32/ESP32 erase granularity rounding fixed in adapters.

### Test Improvements
- `test_uart_full_update_flow`: real CRC32 computation and flash validation.
- `test_uart_null_flush_rejected`: added NULL flush coverage.
- `test_e2e.py` flush handling fixed for Python 3.13.

### Documentation
- `KNOWN_ISSUES.md`, `docs/PROTOCOL_UART.md`, `docs/PLATFORM_STATUS.md`, `RELEASE_CHECKLIST.md`, `EVIDENCE_MATRIX.md` updated in the repository.

### Local Verification in This Workspace
- `cmake -S . -B build`
- `cmake --build build`
- `ctest --test-dir build -C Debug --output-on-failure`
- Result: `15/15` CTest binaries passed.
- GitHub Actions: NOT RUN / unavailable locally.
- ESP32-S3 hardware verification: NOT VERIFIED locally.

### Known Limitations
- ESP32/Xtensa jump mechanism is not implemented.
- STM32/ESP32 adapters require vendor headers and hardware validation.
- Power-loss testing is not implemented.
- Firmware signing is still not implemented.

---

## [v0.6.0] - 2026-06-04

### Critical Bug Fixes
- UART frame decode: fixed the zero-payload minimum size check.
- Session state gating: WRITE/COMMIT/REBOOT now require HELLO first.
- COMMIT validation: firmware size must equal bytes written.
- Frame API NULL checks: input pointers are validated.
- Slot erase timing: moved from session init to first WRITE.
- CMake UART integration: added `LOXBOOT_BUILD_UART_PORT` compile definition.
- Adapter header includes: fixed incorrect include paths in adapter stubs.
- Uninitialized variable: initialized the large payload array in tests.

### Features
- Incremental CRC32 API for streaming verification.
- Full UART session test covering HELLO -> WRITE -> COMMIT -> STATUS -> REBOOT.

### Improvements
- Documentation: added release and verification notes.
- Build verification: warning flags configured in CMake.
- Test expansion: new frame and session validation cases.

---

## [v0.5.0-stm32] - STM32 internal flash adapter
- HAL-based flash operations (read/write/erase)

## [v0.4.0-uart] - UART transport protocol
- Frame format and CRC16-CCITT
- Commands: HELLO, WRITE, COMMIT, ABORT, STATUS, REBOOT

## [v0.3.0-boot-sequence] - Full boot sequence
- 8-step boot sequence with crash loop detection

## [v0.2.0-core] - Core bootloader engine
- CRC32, state management, slot control

## [v0.1.0-spec] - API specification
- Public API and platform adapter interfaces
