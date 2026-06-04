# Test Plan — loxboot v0.6.0

## Test Scope

**13 automated CTests** covering core bootloader functionality and UART transport. Tests run on all 5 CI targets (Ubuntu GCC/Clang, ARM Cortex-M cross-compile, Windows MSVC/Clang-CL).

## Test Breakdown

### Core Tests (v0.2.0–v0.3.0, 11 tests)

#### CRC32 (1 test)
- `test_crc32_known_vectors` — Validates CRC32-CCITT implementation against reference vectors

#### State Management (3 tests)
- `test_loxboot_state_crc` — Verifies dual-copy state read/write with CRC validation
- `test_loxboot_state_recovery` — Validates automatic corruption recovery (uses backup copy when primary is invalid)
- `test_loxboot_state_edges` — Edge cases: empty state, all-zeros state, repeated init

#### Boot Sequence (3 tests)
- `test_loxboot_boot_sequence` — Full 8-step boot (init → active slot → jump)
- `test_loxboot_rollback` — Verifies automatic rollback when boot count exceeds threshold
- `test_loxboot_crash_loop` — Confirms crash loop detection and safe revert

#### Slot Control (2 tests)
- `test_loxboot_slot_control` — commit, invalidate, request operations
- `test_loxboot_slot_state_access` — loxboot_get_slot_state(), boot reason queries

#### Frame Protocol (1 test)
- `test_loxboot_crc16_known_vector` — CRC16-CCITT-FALSE validation (`crc16("123456789") = 0x29B1`)

### UART Transport Tests (v0.4.0, 2 tests + 2 extensions)

#### Session Handling (2 base tests)
- `test_uart_no_hello_timeout` — No CMD_HELLO within listen window → graceful return (LOXBOOT_OK)
- `test_uart_session_init` — Session context initializes with boot context and listen timeout

#### Extensions (2 new tests)
- `test_uart_commands_defined` — Command constants accessible (`LOXBOOT_UART_CMD_HELLO=0x01`, `CMD_WRITE=0x02`)
- `test_uart_session_state` — Session state fields properly initialized

## NOT YET IMPLEMENTED

### Power-Loss Testing
- **Purpose**: Validate dual-copy recovery under power loss during state write
- **Scenario**: Interrupt flash write mid-operation, verify backup copy protects valid state
- **Status**: Requires injection framework (flash write pre-/post-interrupt hooks)

### State-Machine Invariant Tests
- **Purpose**: Validate boot state transitions respect all constraints
- **Constraints**:
  - Only one slot marked active
  - Active slot must be valid (CRC matches)
  - Pending slot (if any) is distinct from active
  - Boot count cannot exceed threshold without rollback
- **Status**: Core validation in src/loxboot_core.c; extended test suite pending

### UART Transfer + Commit (E2E scope)
- **Covered**: HELLO → WRITE (multi-chunk) → COMMIT → REBOOT, slot B = PENDING, firmware bytes in flash, CRC32 verified
- **Negative cases**: Bad CRC, WRITE before HELLO, COMMIT before WRITE, size mismatch, OOB write, NULL flush
- **Status**: IMPLEMENTED — two layers:
  1. `test_loxboot_uart_receive.c::test_uart_full_update_flow` — C unit test with real CRC32, verifies slot B PENDING + firmware bytes in flash
  2. `tools/test_e2e.py` — Python E2E test with loxboot_sim subprocess; 34/34 assertions pass (CTest: `loxboot_e2e`)

**Scope boundary**: E2E tests end at REBOOT + slot PENDING. The next phase — boot selection of PENDING slot, CRC verify, jump, confirm_boot(), VALID — is covered independently by `test_loxboot_boot_sequence.c` and `test_loxboot_rollback.c`. These are deliberately separate: the UART transport layer and the boot sequence are orthogonal concerns.

### Boot Promotion Cycle
- **Covered**: PENDING → boot → attempt counter → CRC verify → jump → confirm_boot() → VALID → rollback on bad CRC
- **Status**: IMPLEMENTED in `test_loxboot_boot_sequence.c`, `test_loxboot_rollback.c`, `test_loxboot_crash_loop.c`
- **NOT covered end-to-end**: Combined flow (UART write → boot → confirm) in one test. This requires hardware or a more complex simulator with jump hooks chained to a second session. Tracked in KNOWN_ISSUES.md.

## Test Execution

### Local (All Platforms)
```bash
cmake -B build -DLOXBOOT_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### CI (5 targets, automated on push/PR)
```yaml
targets:
  - ubuntu-latest / gcc       (Wall -Wextra -Wpedantic -Werror)
  - ubuntu-latest / clang     (Wall -Wextra -Wpedantic -Werror)
  - ubuntu-latest / arm-none-eabi-gcc (Cortex-M4, core-only)
  - windows-latest / msvc     (W4 /WX)
  - windows-latest / clang-cl (Wall -Wextra -Werror)
```

## Compiler Warnings as Errors

All CI targets enforce zero-warning builds:
- **GCC/Clang**: `-Wall -Wextra -Wpedantic -Werror`
- **MSVC**: `/W4 /WX`
- **arm-none-eabi-gcc**: `-Wall -Wextra -Wpedantic -Werror` (Cortex-M4 specific)

## Coverage Gaps

| Feature | Tested | Notes |
|---------|--------|-------|
| State persistence | ✓ | Read/write/recovery tested |
| Boot sequence | ✓ | Full 8-step path + rollback |
| Slot control API | ✓ | All operations (commit, invalidate, request) |
| Crash detection | ✓ | Boot count overflow → rollback |
| CRC32 integrity | ✓ | Polynomial + known vectors |
| CRC16 (UART) | ✓ | Algorithm verified; known vector passes |
| UART session | ✓ | Basic handshake + timeout |
| **STM32 adapter** | ✗ | Requires physical hardware + HAL |
| **ESP32 adapter** | ✗ | Requires physical hardware + IDF |
| **Power-loss recovery** | ✗ | Requires injection framework |
| **Full UART update flow** | ✗ | Requires extended state machine test |
| **State invariants** | Partial | Core logic tested; extended invariant suite pending |

## Next Steps (Post-v0.6.0)

1. Implement power-loss injection framework
2. Add state-machine invariant test suite (5+ tests)
3. Implement full UART update flow tests (10+ negative cases)
4. Add hardware validation tests for STM32/ESP32 (in vendor repos)
5. Performance benchmarks (boot time, UART throughput)
