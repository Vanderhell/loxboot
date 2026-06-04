# Platform Status Report

## Summary

| Platform       | Core | UART    | Adapter      | Handoff           | Status                         |
|----------------|------|---------|--------------|-------------------|--------------------------------|
| Host (tests)   | ✅   | ✅      | ✅ (stubs)   | ✅ (stub-tested)  | VERIFIED (host only)           |
| ARM Cortex-M   | ✅   | ✅      | ⚠️ (no HAL) | ⚠️ (not HW tested)| NOT HARDWARE VERIFIED          |
| STM32          | ✅   | ✅      | ⚠️ (stub)   | ⚠️ (not HW tested)| NOT HARDWARE VERIFIED          |
| ESP32-S3       | ✅   | ✅      | ✅ (IDF)    | ⚠️ (UART only)    | UART PROTOCOL VERIFIED (partial)|
| Xtensa/ESP8266 | ✅   | ✅      | ❌           | ❌                | NO ADAPTER                     |
| RISC-V         | ✅   | ✅      | ❌           | ❌                | NO ADAPTER                     |

**Legend:** ✅ verified  ⚠️ implemented, not hardware-validated  ❌ not implemented

---

## Host (Automated Testing) ✅

**Status:** Verified  
**Tests:** 414/414 assertions, 15 CTest binaries, 0 failures  
**Compiler:** MSVC verified zero warnings; GCC/Clang flags configured

**Coverage (15 test binaries, MSVC local):**
- Boot sequence: 17 (test_loxboot_boot_sequence)
- State management: 132 (test_loxboot_state_edges)
- UART frame: 43 (test_loxboot_uart_frame)
- UART session: 43 (test_loxboot_uart_receive)
- Slot operations: 25 (test_loxboot_invalidate_slot)
- Init/CRC/rollback/format: 37+ (test_loxboot_init)
- Misc slot control: 74
- ESP32 platform (stubs): 15 (test_loxboot_esp32_platform)
- E2E simulator: 34 (loxboot_e2e via Python)

**NOT verified:**
- GCC / Clang CI run (requires GitHub Actions push)
- ASAN / UBSAN run
- ARM cross-compile smoke test (CMake only, no execution)

---

## ARM Cortex-M ⚠️

**Status:** Code present. Jump mechanism implemented. **NOT hardware validated.**

**What works on host:**
- Boot sequence logic (all test assertions pass)
- UART protocol (all assertions pass)
- State management (all assertions pass)

**What requires hardware:**
- `loxboot_jump_to_app()` on real ARM board (MSP setup, Thumb bit, vector table)
- STM32 HAL flash operations (requires vendor stm32_hal.h)
- Power-loss scenarios

---

## STM32 (Internal Flash) ⚠️

**Status:** Adapter code present. Compiles with vendor-provided stub headers. **NOT hardware validated.**

**⚠️ Critical layout requirement:**
Each boot state copy must occupy its own erase page/sector exclusively.
See docs/PORTING.md — Boot state region isolation.

**What the adapter implements:**
- Flash read: memory-mapped direct pointer
- Flash write: 8-byte chunks via `HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD)`
- Flash erase: rounded up to `FLASH_PAGE_SIZE` boundary

**Not verified:**
- Real STM32 HAL (vendor stm32_hal.h required)
- Flash page size (varies by STM32 variant)
- Dual-bank behavior
- Power-loss recovery

---

## ESP32-S3 ⚠️ (partial)

**Status:** UART protocol verified on hardware. Full OTA boot cycle NOT verified.

**Verified on ESP32-S3 DevKit (USB Serial JTAG, IDF v5.5.1):**
- IDF project builds: `idf.py build` → `loxboot_esp32.bin` ✅
- Boot from fresh flash (auto-init state) ✅
- UART listen window active after boot ✅
- HELLO → RSP_STATUS ✅
- WRITE before HELLO → RSP_ERROR ✅
- Corrupt frame (bad CRC) → RSP_ERROR ✅
- COMMIT size mismatch → RSP_ERROR ✅
- STATUS command ✅
- HELLO → WRITE → COMMIT → REBOOT ✅
- slot_b = PENDING after COMMIT ✅
- Device reboots and comes back for next update ✅
- **Hardware E2E: 12/12 assertions pass**

**Platform handoff (stub-tested, not hardware-tested):**
- `loxboot_esp32_handoff()`: maps SLOT_A → ota_0, SLOT_B → ota_1
- Calls `esp_ota_set_boot_partition()` + `esp_restart()`
- `loxboot_esp32_confirm_running_app()`: marks valid only for PENDING_VERIFY
- `loxboot_esp32_sync_state_from_ota()`: maps IDF OTA state to loxboot state
- **Stub tests: 15/15 assertions pass (no ESP-IDF required)**

**NOT verified on hardware:**
- Full OTA boot cycle: UART update → slot PENDING → handoff → IDF bootloader loads ota_1
- App self-test + `esp_ota_mark_app_valid_cancel_rollback()`
- Automatic rollback (bad image → IDF bootloader restores previous)
- Power-loss during update
- `loxboot_esp32_confirm_running_app()` on real device
- `loxboot_esp32_sync_state_from_ota()` on real device

**Required for production ESP32:**
- [ ] OTA update boot cycle smoke test (8 steps from design doc)
- [ ] Rollback test (bad image → confirm rollback)
- [ ] `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` in sdkconfig
- [ ] otadata partition in partitions.csv

---

## Xtensa (ESP8266) ❌

No adapter. Different SDK from ESP32, separate partition API.

---

## RISC-V ❌

No adapter or jump mechanism. External contribution needed.

---

## Build Status

| Target         | Status | Evidence                                    |
|----------------|--------|---------------------------------------------|
| Host MSVC      | ✅     | 414 assertions, 0 failures (local)          |
| Host GCC       | ⚠️    | Flags configured, CI not run yet            |
| Host Clang     | ⚠️    | Flags configured, CI not run yet            |
| ARM cross      | ⚠️    | CMake toolchain configured, no exec test    |
| ESP32-S3 IDF   | ✅     | `idf.py build` → loxboot_esp32.bin (local) |
| STM32 stub     | ⚠️    | Compiles with stubs, not in CI              |
| ESP32 stub     | ✅     | Compiles + 15 stub tests pass               |

**CI status: NOT RUN** — requires `git push origin master && git push origin v0.7.0`

---

## What is verified vs. what is claimed

| Claim                              | Verified by                       | Status  |
|------------------------------------|-----------------------------------|---------|
| Core boot sequence logic correct   | 17 host assertions                | ✅      |
| UART protocol correct              | 43+43+34 host assertions          | ✅      |
| CRC16/CRC32 correct                | Known-vector tests                | ✅      |
| ESP32 UART protocol                | 12 hardware assertions (ESP32-S3) | ✅      |
| ESP32 OTA handoff logic            | 15 stub assertions                | ⚠️     |
| Full ESP32 OTA boot cycle          | Not tested                        | ❌      |
| STM32 flash operations             | Not tested                        | ❌      |
| ARM jump mechanism                 | Not tested                        | ❌      |
| GCC/Clang CI                       | Not run                           | ❌      |
| ASAN/UBSAN                         | Not run                           | ❌      |
