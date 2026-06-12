# Release Checklist - loxboot v0.7.x

This checklist is strict. Do not mark hardware, CI, or release items as complete unless the evidence exists in this repository or in a recorded hardware log.

## Local build/test

- [x] `cmake -S . -B build -DLOXBOOT_BUILD_TESTS=ON -DLOXBOOT_BUILD_UART_PORT=ON`
- [x] `cmake --build build`
- [x] `ctest --test-dir build -C Debug --output-on-failure`
- [x] `cmake --build build --config Release`
- [x] `ctest --test-dir build -C Release --output-on-failure`
- [ ] `tools/local_verify.py`
- [x] Local CTest count recorded in `docs/EVIDENCE_MATRIX.md`

## CI

- [x] GitHub Actions workflow syntax checked
- [x] `workflow_dispatch` present on CI
- [ ] GitHub Actions run in this task

## Release workflow

- [x] Tag-triggered release workflow present
- [ ] Release workflow validated in GitHub Actions
- [ ] GitHub Release created and inspected

## Hardware evidence

- [x] ESP32-S3 OTA E2E log added to `docs/HARDWARE_EVIDENCE.md`
- [x] ESP32-S3 pending-image rollback log added to `docs/HARDWARE_EVIDENCE.md`
- [x] ESP32-S3 corrupt image rejection log added
- [ ] STM32 adapter hardware log added
- [ ] ARM Cortex-M jump log added

## Power-loss/disconnect evidence

- [ ] Disconnect before HELLO
- [ ] Disconnect during first WRITE before erase
- [ ] Disconnect during erase / first WRITE
- [ ] Disconnect during middle WRITE
- [ ] Disconnect after all WRITE before COMMIT
- [ ] Disconnect during COMMIT
- [ ] Disconnect after COMMIT before REBOOT
- [ ] Disconnect during reboot
- [ ] Reconnect and STATUS query
- [x] Power-loss fault injection plan executed or implemented

## Security

- [x] Security architecture plan committed
- [x] CRC32 limitations documented
- [ ] Firmware signing implementation or explicit non-goal recorded
- [ ] No hard dependency on `microcrypt` or `microdh` in core

## Production blockers

- [ ] GitHub Actions remains unverified in this task
- [ ] GitHub Release remains unverified in this task
- [x] ESP32-S3 OTA A/B handoff and rollback evidence is present
- [ ] STM32 hardware evidence is present
- [ ] Power-loss/disconnect evidence is present
- [ ] Firmware signing status is resolved for the intended deployment model
- [ ] Repository still marked `NOT PRODUCTION READY` until all blockers are cleared
