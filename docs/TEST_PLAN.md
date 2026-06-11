# Test Plan - loxboot v0.7.x

This document records the current automated test scope and the evidence that is present in this repository. It does not claim hardware verification unless a hardware log is added in `docs/HARDWARE_EVIDENCE.md`.

## Host automated tests

Current CTest count with `-DLOXBOOT_BUILD_UART_PORT=ON`:
- 11 core tests
- 3 UART tests
- 1 Python E2E test
- Total: 15

Representative local command:

```bash
cmake -S . -B build -DLOXBOOT_BUILD_TESTS=ON -DLOXBOOT_BUILD_UART_PORT=ON
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

Covered host-side areas:
- CRC32
- state management
- boot sequence and rollback
- slot control
- UART frame handling
- UART session handling
- E2E UART simulator flow

## CI tests

The repository has CI workflows for:
- `push` and `pull_request` builds in `.github/workflows/ci.yml`
- tag-triggered release packaging in `.github/workflows/release.yml`

Status in this task:
- CI workflow syntax and intent: prepared locally
- CI execution: not run in this task
- Release execution: not run in this task

## ESP32-S3 hardware tests

Harnesses present in the repository:
- `tools/test_e2e_ota.py`
- `idf_project/`
- ESP32 adapter code under `adapters/esp32/`

The ESP32-S3 OTA harness was run in this task and passed after aligning the expectations with the device's actual slot-selection behavior.

The hardware coverage was exercised in two modes:
- auto-confirm enabled: verified OTA A/B handoff and corrupt-image rejection
- auto-confirm disabled (`LOXBOOT_ESP32_AUTO_CONFIRM=0`): verified pending-image rollback on the second reboot

## Power-loss / disconnect tests

Current state:
- Planned
- Not verified in this task
- No hardware log committed in this task

The repository still needs evidence for:
- disconnect before HELLO
- disconnect during the first WRITE before erase
- disconnect during erase
- disconnect during a middle WRITE
- disconnect after all WRITE commands but before COMMIT
- disconnect during COMMIT
- disconnect after COMMIT but before REBOOT
- disconnect during reboot
- reconnect and STATUS query
- disconnect / power-loss fault injection

## Security/signing tests

Current state:
- CRC32 corruption detection is implemented
- firmware authenticity is not implemented
- signing tests are not present

## Not verified in this task

- GitHub Actions
- GitHub Release
- ESP32 dongle disconnect / power-loss scenarios
- STM32 hardware
- firmware signing
