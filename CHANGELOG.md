# Changelog

All notable changes to loxboot are documented in this file.

## [v0.7.0] — 2026-06-04

### New Features
- **`loxboot_format_state()` public API** — Factory provisioning: writes blank boot state to flash before first `loxboot_run()`. Replaces internal-only `loxboot_state_make_default()`.
- **Fresh flash auto-recovery** — `loxboot_run()` now handles `LOXBOOT_ERR_RECORD_CORRUPT` by initializing blank state instead of calling `on_fatal()`. Fresh devices boot without manual provisioning.
- **E2E device simulator** (`tools/loxboot_sim.c`) — Standalone binary implementing loxboot device-side UART protocol over stdin/stdout. Used for integration testing without hardware.
- **Python E2E test suite** (`tools/test_e2e.py`) — 34 protocol assertions against real loxboot code via simulator subprocess. Wired into CTest as `loxboot_e2e`.
- **Hardware E2E test** (`tools/test_e2e_serial.py`) — 12 assertions on real ESP32-S3 via USB Serial JTAG (COM19). Verified against actual hardware.
- **ESP32-S3 IDF project** (`idf_project/`) — Complete ESP-IDF v5.5 integration: USB Serial JTAG transport, partition table, sector-aligned erase, 8KB stack.
- **Release workflow** (`.github/workflows/release.yml`) — 3-job pipeline: gate (5-target CI), package (source + headers + Windows MSVC .lib), publish (GitHub Release with CHANGELOG excerpt).
- **Usage examples** (`examples/`) — STM32, ESP32, and generic bare-metal adapter templates.

### Bug Fixes
- **UART frame decode:** Minimum frame size 7→6 bytes (zero-payload frames rejected incorrectly)
- **Session gating:** WRITE/COMMIT/REBOOT now require HELLO first
- **COMMIT size validation:** `firmware_size` must equal `_bytes_written`
- **flush NULL check:** `loxboot_uart_run_session()` validates `transport.flush != NULL`
- **session.transport dead field:** Removed unused `transport` field from `loxboot_uart_session_t`
- **STM32 int-to-pointer cast:** Fixed `-Werror=int-to-pointer-cast` in `loxboot_flash_stm32.c`
- **STM32/ESP32 erase granularity:** Both adapters now round erase requests up to sector/page boundary
- **ESP32 fresh-flash panic:** Stack overflow fixed (3584→8192 bytes), erase granularity fixed, USB JTAG transport used correctly
- **CMake UART integration:** `LOXBOOT_BUILD_UART_PORT=1` now propagates to compile definitions

### Test Improvements
- `test_uart_full_update_flow`: Replaced stub with real CRC32 computation, slot B PENDING verification, and firmware byte validation in flash
- `test_uart_null_flush_rejected`: Added test for NULL flush callback rejection
- `test_e2e.py finish()`: Fixed `ValueError: flush of closed file` on Python 3.13
- Boot sequence test updated: corrupt state → `NO_VALID_SLOT` (correct behavior after auto-recovery)

### Documentation
- `KNOWN_ISSUES.md` — Comprehensive list of limitations and hardware validation gaps
- `docs/PROTOCOL_UART.md` — Fixed CRC16 final XOR (0x0000 not 0xFFFF), slot erase timing
- `docs/PLATFORM_STATUS.md` — Accurate per-platform build and test status
- `docs/TEST_PLAN.md` — Updated to reflect E2E test implementation
- `RELEASE_CHECKLIST.md`, `EVIDENCE_MATRIX.md` — RC hardening evidence
- `CLAUDE.md` — Project-level Claude Code instructions (full autonomy)

### CI/Build
- 5-target CI matrix: Ubuntu GCC, Ubuntu Clang, Ubuntu ARM cross-compile, Windows MSVC, Windows ClangCL
- Release pipeline: source archive + header bundle + Windows MSVC `.lib` + SHA256SUMS
- `.gitignore` extended: `idf_project/build/`, `sdkconfig`, `*.zip`, `build_*/`

### Test Results
```
Boot sequence:         17 assertions ✅
State management:     132 assertions ✅
UART frame:            43 assertions ✅
UART session:          43 assertions ✅
Slot operations:       25 assertions ✅
Init/CRC/rollback:     37 assertions ✅
Misc:                  74 assertions ✅
E2E (simulator):       34 assertions ✅ (CTest: loxboot_e2e)
────────────────────────────────────────
CTest binaries:        14/14 pass
Total assertions:     371 ✅ (100% pass rate)
Hardware (ESP32-S3):  12/12 ✅
```

### Known Limitations (unchanged)
- ESP32/Xtensa jump mechanism not implemented (`loxboot_run()` jump is Cortex-M style)
- STM32/ESP32 adapters require vendor headers — not buildable standalone
- Power-loss testing not implemented
- Erase granularity handled by adapters — see `KNOWN_ISSUES.md`

---

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
- **Test expansion:** 336 → 366 tests with new frame/session validation cases

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
Total:                366 tests ✅ (100% pass rate)
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
