# Platform Status Report

## Summary

| Platform | Core | UART | Adapter | Handoff | Status |
|---|---|---|---|---|---|
| Host (tests) | ✅ | ✅ | ✅ (stubs) | ✅ (stub-tested) | VERIFIED LOCALLY |
| ARM Cortex-M | ✅ | ✅ | ⚠️ (no HAL) | ⚠️ (not HW tested) | NOT HARDWARE VERIFIED |
| STM32 | ✅ | ✅ | ⚠️ (stub) | ⚠️ (not HW tested) | NOT HARDWARE VERIFIED |
| ESP32-S3 | ✅ | ✅ | ✅ (IDF) | ⚠️ (UART only) | NOT VERIFIED LOCALLY |
| Xtensa/ESP8266 | ✅ | ✅ | ⛔ | ⛔ | NO ADAPTER |
| RISC-V | ✅ | ✅ | ⛔ | ⛔ | NO ADAPTER |

**Legend:** ✅ verified locally, ⚠️ implemented but not hardware-validated, ⛔ not implemented.

---

## Host Verification

**Status:** Verified locally in this workspace

**Tests:** 15/15 CTest binaries, 0 failures

**Full local test profile:** `cmake -S . -B build -DLOXBOOT_BUILD_UART_PORT=ON`

**Build:** MSBuild Debug configuration passed in this workspace

**Not verified locally:**
- GitHub Actions
- GCC/Clang runs
- ASAN/UBSAN
- ARM cross-compile execution

---

## ARM Cortex-M

**Status:** Code present. Hardware jump behavior is not verified locally.

**What is covered locally:**
- Boot sequence logic
- UART protocol
- State management

**Not verified locally:**
- `loxboot_jump_to_app()` on a real ARM board
- STM32 HAL flash operations on real hardware
- Power-loss scenarios

---

## STM32

**Status:** Adapter code present. Stub compilation passed locally; hardware validation did not run in this task.

**Adapter behavior in code:**
- Flash read: memory-mapped direct pointer
- Flash write: 8-byte chunks via `HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD)`
- Flash erase: rounded up to `FLASH_PAGE_SIZE`

**Not verified locally:**
- Real STM32 HAL
- Flash page size by variant
- Dual-bank behavior
- Power-loss recovery

---

## ESP32-S3

**Status:** Local stub tests passed. Hardware UART/OTA behavior was not verified locally in this task.

**Stub coverage in this workspace:**
- `loxboot_esp32_handoff()`
- `loxboot_esp32_confirm_running_app()`
- `loxboot_esp32_sync_state_from_ota()`
- `test_loxboot_esp32_platform`

**Not verified locally:**
- Full OTA boot cycle
- App rollback behavior on a real device
- Power-loss during update
- `esp_ota_mark_app_valid_cancel_rollback()` on real hardware

---

## Build Status

| Target | Status | Evidence |
|---|---|---|
| Host MSVC | ✅ | 15/15 CTest binaries, 0 failures |
| Host GCC | ⚠️ | Flags configured, not run locally |
| Host Clang | ⚠️ | Flags configured, not run locally |
| ARM cross | ⚠️ | CMake toolchain configured, not executed |
| ESP32-S3 IDF | ⚠️ | Not verified locally in this task |
| STM32 stub | ✅ | Stub build passed locally |
| ESP32 stub | ✅ | Local stub tests passed |

**GitHub Actions: NOT RUN / unavailable locally**

---

## Verified vs. Not Verified

| Claim | Local evidence | Status |
|---|---|---|
| Core boot sequence logic | 15/15 CTest binaries | ✅ |
| UART protocol logic | 15/15 CTest binaries | ✅ |
| CRC16/CRC32 code paths | Known-vector tests in repo | ✅ |
| ESP32 UART protocol | Not verified locally | ⚠️ |
| ESP32 OTA handoff logic | Stub tests only | ⚠️ |
| Full ESP32 OTA boot cycle | Not tested locally | ⛔ |
| STM32 flash operations | Not tested locally | ⛔ |
| ARM jump mechanism | Not tested locally | ⛔ |
| GitHub Actions | NOT RUN / unavailable locally | ⛔ |
| ASAN/UBSAN | Not run locally | ⛔ |
