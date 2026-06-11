# Platform Status Report

## Summary

| Platform | Core | UART | Adapter | Handoff | Status |
|---|---|---|---|---|---|
| Host tests | verified | verified | stubs | stub-tested | VERIFIED LOCALLY |
| ARM Cortex-M | verified | verified | not hardware-verified | not hardware-verified | CODE PRESENT, HARDWARE EVIDENCE MISSING |
| STM32 | verified | verified | present | not hardware-verified | ADAPTER PRESENT, HARDWARE EVIDENCE MISSING |
| ESP32-S3 | verified | verified | present | partially verified | OTA E2E verified; disconnect / power-loss evidence missing |
| Xtensa/ESP8266 | verified | verified | no adapter | no handoff | NO ADAPTER |
| RISC-V | verified | verified | no adapter | no handoff | NO ADAPTER |

## Host Verification

Status:
- Verified locally in this workspace
- 15/15 CTest binaries passed with `-DLOXBOOT_BUILD_UART_PORT=ON`

Not verified in this task:
- GitHub Actions
- GitHub Release
- GCC/Clang local runs
- ARM hardware
- STM32 hardware

## ARM Cortex-M

Status:
- Code present
- Hardware jump behavior not verified in this task

Not verified:
- `loxboot_jump_to_app()` on a real board
- real flash erase/write behavior
- power-loss scenarios

## STM32

Status:
- Adapter code present
- Stub-friendly build path exists

Not verified:
- real STM32 HAL integration
- flash page size by variant
- dual-bank behavior
- power-loss recovery

## ESP32-S3

Status:
- Adapter code present
- OTA harness present
- OTA E2E verified in this task
- Disconnect / power-loss behavior not verified

Not verified:
- disconnect / power-loss behavior
- rollback behavior on a real device
- `esp_ota_mark_app_valid_cancel_rollback()` on real hardware

## Verification Matrix

| Claim | Local evidence | Status |
|---|---|---|
| Core boot sequence logic | 15/15 CTest binaries | VERIFIED |
| UART protocol logic | 15/15 CTest binaries | VERIFIED |
| CRC16/CRC32 code paths | Known-vector tests in repo | VERIFIED |
| ESP32 UART protocol | No hardware log in this task | NOT VERIFIED |
| ESP32 OTA handoff logic | Harness present, no hardware log | NOT VERIFIED |
| Full ESP32 OTA boot cycle | No hardware log in this task | NOT VERIFIED |
| STM32 flash operations | No hardware log in this task | NOT VERIFIED |
| ARM jump mechanism | No hardware log in this task | NOT VERIFIED |
| GitHub Actions | Not run in this task | NOT VERIFIED |
| GitHub Release | Not verified in this task | NOT VERIFIED |
