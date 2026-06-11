# Known Issues and Limitations

This file only lists unresolved items. Do not remove an item unless the fix is implemented and tested.

## Current blockers

- GitHub Actions was not run in this task
- GitHub Release was not verified in this task
- ESP32-S3 OTA A/B handoff and rollback are verified, but disconnect / power-loss hardware evidence is still missing
- STM32 hardware verification is missing
- power-loss during erase/write/state-write is not fully verified
- UART retry/resume support is missing
- firmware signing is missing

## Critical issues

### 1. Erase granularity mismatch

Status: documented, still requires platform-specific handling

Impact: boot state corruption can occur on platforms with large flash sectors if the adapter does not round the erase range correctly.

Current behavior:
- core writes boot state records that are much smaller than a flash erase sector
- adapters must erase full sectors or pages as required by the platform
- slot and state layout must avoid overlapping any adjacent data

Resolution options:
1. API change: add erase granularity to platform configuration
2. Adapter workaround: allocate a full sector for each boot-state copy

Recommended for the current release line: adapter-side sector allocation or equivalent platform-owned layout

### 2. ARM Cortex-M jump mechanism

Status: code present, hardware validation pending

Impact: device may not jump into the application correctly on real hardware.

What works in code:
- jump routine exists
- stack pointer initialization is implemented
- interrupts are disabled before the jump

What still needs hardware evidence:
- real STM32 or other ARM board execution
- interrupt masking in hardware
- entry point alignment and memory layout assumptions

### 3. Power-loss recovery

Status: mocked behavior exists, real hardware scenarios are unverified

Impact: data corruption or an unbootable device may occur if power is lost during update.

Scenarios not fully verified:
- power loss during slot erase
- power loss during firmware write
- power loss during boot-state write
- interrupt during state write
- mid-erase corruption patterns
- recovery from corrupted boot state on boot

### 4. Flash adapter integration

Status: stubbed for host builds, real hardware untested

STM32 adapter:
- code is present
- real HAL integration is not verified
- flash page size varies by variant

ESP32 adapter:
- code is present
- ESP-IDF integration is not hardware verified
- partition table must match slot definitions

### 5. UART frame loss and corruption

Status: timeout-based recovery only

Current behavior:
- per-byte timeout aborts a frame
- no retry mechanism is implemented
- corrupted frames must be retransmitted by restarting the transfer

Limitation:
- this is not production-grade retry/resume support
- noisy serial lines can still force the host to restart the transfer

## Documentation gaps

### 6. Memory layout examples

Status: documented, but still incomplete for some boards

### 7. CRC32 assumptions

Status: current implementation uses flash reads in chunks; platform-specific tuning may still be needed

## Future improvements

### 8. Signed firmware updates

Status: missing

Current verification only detects accidental corruption. It does not prove authenticity.

## Test matrix status

| Scenario | Mock test | Hardware test | Status |
|---|---|---|---|
| Boot from A | yes | no | ready for hardware |
| Boot from B | yes | no | ready for hardware |
| Update A to B | yes | no | ready for hardware |
| Update B to A | yes | no | ready for hardware |
| Rollback on CRC | yes | no | ready for hardware |
| Crash loop auto-rollback | yes | no | ready for hardware |
| Dual-copy recovery | yes | no | ready for hardware |
| Power loss during erase | no | no | missing |
| Power loss during write | no | no | missing |
| Partial frame loss | yes | no | ready for hardware |
| UART timeout | yes | no | ready for hardware |
| NULL adapter callbacks | yes | n/a | complete |

## Verification checklist before production

- [ ] Hardware flash erase/write/read verified on target
- [ ] Hardware jump to app works on real board
- [ ] Hardware power-loss scenarios tested in a lab
- [ ] Hardware UART update end-to-end on real serial port
- [ ] Hardware boot reason tracking validates correctly
- [ ] Security: firmware signing implemented if needed
- [ ] Documentation: platform-specific memory layout documented
- [ ] Documentation: adapter implementation guide completed
- [ ] Testing: all hardware test scenarios passed
