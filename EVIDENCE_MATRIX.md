# Evidence Matrix - v0.7.0 Verification

## Overview

This matrix tracks the requirements that are backed by code in this repository and by local test execution in this workspace.

---

## Core Boot Sequence

| Requirement | Code | Test evidence | Status |
|---|---|---|---|
| `loxboot_run()` boot sequence | `src/loxboot_core.c` | `test_loxboot_boot_sequence.c` | ✅ |
| Crash loop rollback | `src/loxboot_core.c` | `test_loxboot_crash_loop.c`, `test_loxboot_rollback.c` | ✅ |
| Confirm boot resets attempts | `src/loxboot_core.c` | `test_loxboot_confirm_boot.c` | ✅ |
| State validation rejects invalid `active_slot` | `src/loxboot_state.c` | `test_loxboot_state_edges.c` | ✅ |

---

## UART Transport

| Requirement | Code | Test evidence | Status |
|---|---|---|---|
| Frame encode/decode roundtrip | `ports/uart/loxboot_uart.c` | `test_loxboot_uart_frame.c` | ✅ |
| HELLO gating for WRITE/COMMIT/REBOOT | `ports/uart/loxboot_uart.c` | `test_loxboot_uart_receive.c` | ✅ |
| COMMIT size validation | `ports/uart/loxboot_uart.c` | `test_loxboot_uart_receive.c` | ✅ |
| Overflow-safe WRITE bounds check | `ports/uart/loxboot_uart.c` | `test_loxboot_uart_receive.c` | ✅ |
| NULL pointer checks | `ports/uart/loxboot_uart.c` | `test_loxboot_uart_frame.c` | ✅ |

---

## Firmware Verification

| Requirement | Code | Test evidence | Status |
|---|---|---|---|
| CRC32 known vector (`123456789` -> `0xCBF43926`) | `src/loxboot_crc32.c` | `test_loxboot_crc32.c` | ✅ |
| Incremental CRC32 API | `src/loxboot_crc32.c` | `test_loxboot_boot_sequence.c`, `test_loxboot_uart_receive.c` | ✅ |
| Firmware CRC via `flash.read()` | `src/loxboot_core.c` | `test_loxboot_boot_sequence.c` | ✅ |

---

## Slot Management

| Requirement | Code | Test evidence | Status |
|---|---|---|---|
| Commit slot | `src/loxboot_core.c` | `test_loxboot_commit_slot.c` | ✅ |
| Invalidate slot | `src/loxboot_core.c` | `test_loxboot_invalidate_slot.c` | ✅ |
| Request slot | `src/loxboot_core.c` | `test_loxboot_request_slot.c` | ✅ |
| `loxboot_format_state()` | `src/loxboot_core.c` | `test_loxboot_init.c` | ✅ |

---

## Build and Verification

| Requirement | Evidence | Status |
|---|---|---|
| MSVC build | Local `cmake --build build` | ✅ |
| Local CTest | Local `ctest --test-dir build -C Debug --output-on-failure` | ✅ (`15/15`) |
| GitHub Actions | Not run locally | ⛔ |
| Hardware validation | Not verified locally | ⛔ |

---

## Known Gaps

| Requirement | Status | Notes |
|---|---|---|
| ARM Cortex-M jump mechanism | Not verified locally | No hardware run in this task |
| STM32 flash operations | Not verified locally | Stub build only |
| ESP32-S3 OTA boot cycle | Not verified locally | Stub tests passed; hardware run not done |
| Power-loss recovery | Not verified locally | No lab run in this task |
| Firmware signing | Missing | Still CRC32 only |

---

## Critical Fixes Applied in This Session

| Issue | Fix | Evidence |
|---|---|---|
| Checked range end arithmetic | Added overflow-safe helpers in `src/loxboot_core.c` | Build + local tests pass |
| Invalid `active_slot` in persisted state | Rejected in `src/loxboot_state.c` validation | `test_loxboot_state_edges.c` |
| Non-ARM default handoff | Fails safely without explicit handoff | `test_loxboot_boot_sequence.c` |
| Large stack firmware buffer | Reduced to `LOXBOOT_FW_VERIFY_CHUNK_SIZE` | Build + local tests pass |
| UART WRITE overflow | Rejected before wraparound | `test_loxboot_uart_receive.c` |

---

## Verdict

Local verification in this workspace:
- `15/15` CTest binaries passed.
- `GitHub Actions: NOT RUN / unavailable locally`.
- Hardware validation remains incomplete.

The repository remains `NOT PRODUCTION READY`.
