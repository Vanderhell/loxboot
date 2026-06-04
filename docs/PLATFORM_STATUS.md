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
**Tests:** 363/363 passing (362 core + 1 flush validation)  
**Compiler:** MSVC verified zero warnings; GCC/Clang flags configured

**Coverage:**
- Boot sequence: 17 tests
- State management: 132 tests (corruption recovery)
- UART protocol: 77 tests (frame + session + flush validation)
- Slot operations: 25 tests
- Init/CRC: 37 tests
- UART session boundary: 75 tests

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

**Status:** Adapter code complete, needs validation  
**Adapter:** `adapters/stm32/loxboot_flash_stm32.c`

**What works (in theory):**
- Flash read (memory-mapped)
- Flash write (8-byte chunks via HAL)
- Flash erase (page-aligned sectors)

**What needs testing:**
- Real STM32 HAL integration (`stm32_hal.h` header)
- Flash page size assumptions
- Flash dual-bank behavior (if applicable)
- Erase granularity with boot state (52 bytes → sector align)
- Write timing (program/erase cycles)

**Known issues:**
- State erase assumes sector granularity (STM32 enforces)
- Page size varies by STM32 variant
- Dual-bank models may need special handling

**Next steps:**
1. Provide real `stm32_hal.h` (user's HAL)
2. Configure `FLASH_PAGE_SIZE` for target STM32
3. Test flash operations with real memory
4. Verify boot state backup copies work
5. Run power-loss scenarios

**Build:**
```bash
cmake -DLOXBOOT_BUILD_STM32_ADAPTER=ON -DSTM32_HAL_PATH=/path/to/hal ..
cmake --build . --config Release
```

## ESP32 (esp_partition API) ⚠️

**Status:** Adapter code complete, needs validation  
**Adapter:** `adapters/esp32/loxboot_flash_esp32.c`

**What works (in theory):**
- Partition discovery via esp_partition_find_first()
- Flash read via esp_partition_read()
- Flash write via esp_partition_write()
- Flash erase via esp_partition_erase_range()

**What needs testing:**
- Real ESP32 partition table (`partitions.csv`)
- esp_partition API on actual IDF version
- Sector alignment (typically 4KB)
- Read-only vs. read-write partitions
- OTA partition compatibility
- Power-loss during partition operations

**Known issues:**
- Assumes partition handle is stable
- No built-in recovery if partition moves
- Write assumes pre-erase (adapter enforces)

**Next steps:**
1. Create partitions.csv with slots A/B
2. Flash loxboot + partitions to ESP32
3. Test esp_partition API calls
4. Verify boot state copies persist
5. Run OTA update cycle

**Build:**
```bash
# In ESP-IDF project
idf.py add_component /path/to/loxboot
cmake -DLOXBOOT_BUILD_ESP32_ADAPTER=ON ..
idf.py build
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
| Host (MSVC) | ✅ | 363 tests passing, zero warnings |
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
