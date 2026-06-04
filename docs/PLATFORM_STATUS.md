# Platform Status Report

## Summary

| Platform | Core | UART | Adapter | Status |
|----------|------|------|---------|--------|
| Host (tests) | ✅ | ✅ | ✅ | Ready |
| ARM Cortex-M | ✅ | ✅ | ⚠️ | Needs jump validation |
| STM32 | ✅ | ✅ | ⚠️ | Needs real hardware |
| ESP32 | ✅ | ✅ | ⚠️ | Needs real hardware |
| Xtensa (ESP8266) | ✅ | ✅ | ❌ | No adapter |
| RISC-V | ✅ | ✅ | ❌ | No adapter |

## Host (Automated Testing) ✅

**Status:** Hardened bootloader core for testing  
**Tests:** 366/366 passing  
**Compiler:** MSVC verified zero warnings; GCC/Clang flags configured (-Wall -Wextra -Wpedantic -Werror)

**Coverage (13 test binaries):**
- Boot sequence: 17 assertions (test_loxboot_boot_sequence)
- State management: 132 assertions (test_loxboot_state_edges)
- UART frame: 43 assertions (test_loxboot_uart_frame)
- UART session: 38 assertions (test_loxboot_uart_receive)
- Slot operations: 25 assertions (test_loxboot_invalidate_slot)
- Init/CRC/rollback: 37 assertions
- Misc slot control: 74 assertions

## ARM Cortex-M ⚠️

**Status:** Code complete, jump mechanism needs validation  
**Adapter:** None (jump is core responsibility)

**What works:**
- Boot sequence
- UART transport
- State management
- Firmware CRC verification

**What needs testing:**
- `loxboot_jump_to_app()` on real hardware
- Stack pointer and entry point setup
- Interrupts disabled before jump
- Memory layout assumptions

**Hardware needed:**
- STM32 board (any Cortex-M variant)
- UART interface
- Power-loss injection (optional)

**Next steps:**
1. Load loxboot on STM32
2. Run boot sequence to first app jump
3. Verify app receives control
4. Test UART update sequence
5. Verify firmware update → new app boots

## STM32 (Internal Flash) ⚠️

**Status:** Adapter code present. Verified to compile with vendor-provided stub headers. **Not hardware-validated.**  
**Adapter:** `adapters/stm32/loxboot_flash_stm32.c`

**What the adapter implements:**
- Flash read via memory-mapped direct pointer
- Flash write in 8-byte chunks via `HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD)`
- Flash erase rounded up to `FLASH_PAGE_SIZE` boundary

**⚠️ Critical layout requirement:**
Each boot state copy (primary + backup) **must occupy its own erase page/sector exclusively**.
The adapter rounds erase requests up to `FLASH_PAGE_SIZE`. If `boot_state_primary_base` shares a page with application code, firmware, config, or any other data, that data **will be erased** when loxboot updates boot state.

Minimum safe layout example (assuming 2KB pages):
```
0x08004000 — boot state primary  (one full 2KB page, nothing else)
0x08004800 — boot state backup   (one full 2KB page, nothing else)
0x08005000 — [application or slot A start]
```

**What requires real hardware:**
- STM32 HAL headers (`stm32_hal.h` must be provided by user)
- `FLASH_PAGE_SIZE` varies by STM32 variant (512B / 1KB / 2KB / 16KB / 128KB)
- Dual-bank models need special `Banks` field configuration
- Write timing and program cycle verification
- Power-loss scenarios

**Build (requires vendor HAL):**
```bash
cmake -DLOXBOOT_BUILD_STM32_ADAPTER=ON ..
# stm32_hal.h and stm32_hal.c must be in your include path
```

## ESP32 (esp_partition API) ⚠️

**Status:** Adapter code present. IDF project (`idf_project/`) builds and runs on ESP32-S3 DevKit. UART protocol verified on hardware (12/12 E2E assertions). **Full boot cycle (jump to app) not tested.**  
**Adapter:** `adapters/esp32/loxboot_flash_esp32.c`

**Verified on hardware (ESP32-S3, IDF v5.5.1):**
- UART protocol: HELLO/WRITE/COMMIT/REBOOT/STATUS/ABORT — all correct
- Boot state write to `loxstate`/`loxbkup` partitions
- slot B = PENDING after COMMIT
- Device survives REBOOT and responds to next listen window

**⚠️ Critical layout requirement:**
Each boot state partition (`loxstate`, `loxbkup`) must be an independent partition. The adapter rounds erase requests up to 4096 bytes (one flash sector). If the partition is smaller or shares an erase block, data loss will occur.

Minimum safe partition sizes:
```csv
loxstate,   data, 0x40,  0x10000,  0x1000,   # 4KB minimum
loxbkup,    data, 0x41,  0x11000,  0x1000,   # 4KB minimum
```

**Architecture change in v0.7.0:**
- `loxboot_run()` now uses `platform_ops.handoff()` callback instead of hardcoded ARM jump
- ESP32 platform layer (`adapters/esp32/loxboot_esp32_platform.c`) implements handoff via `esp_ota_set_boot_partition()` + `esp_restart()`
- ARM Cortex-M vector-table jump is the fallback when `platform_ops.handoff == NULL`
- ESP32 handoff tested with stubs: 15/15 assertions pass (host, no ESP-IDF required)

**Not yet verified on hardware:**
- End-to-end boot after UART update (UART → slot PENDING → handoff → IDF bootloader loads new app)
- Power-loss scenarios
- OTA partition compatibility
- Flash write verify (esp_partition_read after write)

**Build (requires ESP-IDF):**
```bash
cd idf_project
idf.py set-target esp32s3
idf.py build
idf.py -p COM19 flash
```

## Xtensa (ESP8266) ❌

**Status:** No adapter  
**Reason:** Requires separate esp8266_rtos_sdk (incompatible with ESP32 IDF)

**Future work:**
- Separate adapter using esp8266 partition API
- Different flash layout than ESP32
- Simpler (single-bank) architecture

## RISC-V ❌

**Status:** No adapter or jump mechanism  
**Reason:** No reference hardware or test environment

**Future work:**
- Generic RISC-V jump template (stack pointer, PC load)
- SiFive or other RISC-V board support
- External contribution needed

## Testing Checklist by Platform

### Host Tests (Automated, all passing)
- [ ] Boot sequence
- [ ] State corruption recovery
- [ ] UART protocol
- [ ] Session state machine
- [ ] Slot operations
- [ ] CRC verification

### ARM Cortex-M (Manual)
- [ ] UART connection and timeout
- [ ] Boot sequence execution
- [ ] Jump to app address
- [ ] App receives control
- [ ] UART update cycle
- [ ] Power-loss during boot
- [ ] Power-loss during update
- [ ] Rollback on bad CRC

### STM32 (Manual)
- [ ] All ARM Cortex-M tests
- [ ] Flash erase works
- [ ] Flash write works
- [ ] Flash read works
- [ ] Dual-copy state recovery on corrupt flash
- [ ] Boot from Slot A, update to Slot B
- [ ] Boot from Slot B, update to Slot A

### ESP32 (Manual)
- [ ] All ARM Cortex-M tests
- [ ] Partition discovery
- [ ] Flash erase/write/read via partition API
- [ ] OTA update cycle
- [ ] Power-loss during partition write
- [ ] Recovery with corrupted boot state

## Build Status

| Target | Build Status | Notes |
|--------|--------------|-------|
| Host (MSVC) | ✅ | 366 tests passing, zero warnings |
| Host (GCC) | ✅ | Core-only cross-compile verified |
| Host (Clang) | ✅ | Flags configured, not verified on Windows |
| STM32 stub | ⚠️ | Fixed int-to-pointer cast, needs stub headers |
| ESP32 stub | ✅ | With esp_partition.h stub |
| STM32 real | ⏳ | Awaits user STM32 + real HAL |
| ESP32 real | ⏳ | Awaits user ESP32 + IDF |

## Recommendations

1. **For development:** Use host build for algorithm/protocol development
2. **For first deployment:** Use STM32 (most common, well-documented flash)
3. **For ESP32 use:** Integrate with ESP-IDF project, test OTA cycle
4. **For production:** Validate all power-loss scenarios on target hardware
