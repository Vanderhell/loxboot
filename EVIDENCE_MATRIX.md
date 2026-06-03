# Evidence matrix

| Area | Evidence | Verified by | Status | Limitations |
|---|---|---|---|---|
| **v0.1.0-spec** | | | | |
| Public API: loxboot.h | `include/loxboot/loxboot.h` | Manual audit | VERIFIED | Spec-only; no implementation |
| Public API: loxboot_transport.h | `include/loxboot/loxboot_transport.h` | Manual audit | VERIFIED | Spec-only; no implementation |
| Public API: loxboot_version.h | `include/loxboot/loxboot_version.h` | Manual audit | VERIFIED | Version constants only |
| Slot model spec | `docs/SPEC.md` §4 | Manual audit | VERIFIED | Spec-only |
| Boot state dual-copy spec | `docs/SPEC.md` §5 | Manual audit | VERIFIED | Spec-only |
| Boot sequence spec | `docs/SPEC.md` §6 | Manual audit | VERIFIED | Spec-only |
| Crash loop detection spec | `docs/SPEC.md` §7 | Manual audit | VERIFIED | Spec-only |
| Rollback spec | `docs/SPEC.md` §8 | Manual audit | VERIFIED | Spec-only |
| Boot confirmation spec | `docs/SPEC.md` §9 | Manual audit | VERIFIED | Spec-only |
| Boot reason spec | `docs/SPEC.md` §10 | Manual audit | VERIFIED | Spec-only |
| UART protocol spec | `docs/SPEC.md` §11 | Manual audit | VERIFIED | Spec-only; v0.4.0 implementation |
| Adapter interface spec | `docs/SPEC.md` §12 | Manual audit | VERIFIED | Spec-only |
| Error model | `docs/SPEC.md` §13 + `include/loxboot/loxboot.h` | Manual audit | VERIFIED | Spec-only |
| Zero heap constraint | No malloc/calloc/realloc/free in `include/` or `src/` | Static search (`grep -RInE`) | VERIFIED | Search does not cover generated build artifacts |
| No internal name leaks | No internal names in `include/` | Static search | VERIFIED | Headers only at v0.1.0 |
| C99 compliance | No C99+ features in headers | Manual audit | VERIFIED | Headers only |
| CMakeLists.txt (INTERFACE→STATIC) | `CMakeLists.txt` | Manual audit | VERIFIED | Builds STATIC when src/ exists |
| CI workflow | `.github/workflows/ci.yml` | Manual audit | VERIFIED | Not run until v0.2.0 |
| **v0.2.0-core** | | | | |
| loxboot_crc32 implementation | `src/loxboot_crc32.c` | Local builds (MSVC + clang-cl Debug/Release) | VERIFIED | GCC/Clang (Linux) not verified here |
| loxboot_crc32 tests | `tests/test_loxboot_crc32.c` | CTest (MSVC + clang-cl Debug/Release) | VERIFIED | GCC/Clang (Linux) not verified here |
| loxboot_init implementation | `src/loxboot_core.c` | Local builds (MSVC + clang-cl Debug/Release) | VERIFIED | GCC/Clang (Linux) not verified here |
| loxboot_init tests | `tests/test_loxboot_init.c` | CTest (MSVC + clang-cl Debug/Release) | VERIFIED | GCC/Clang (Linux) not verified here |
| Boot state read/write | `src/loxboot_state.c` | CTest (MSVC + clang-cl Debug/Release) | VERIFIED | Exercises read/restore/write paths via RAM flash |
| Slot state tests | `tests/test_loxboot_slot_state.c` | CTest (MSVC Debug/Release) | VERIFIED | In-RAM state only (no loxboot_run) |
| loxboot_confirm_boot | `src/loxboot_core.c` | CTest (MSVC Debug/Release) | VERIFIED | No loxboot_run yet |
| loxboot_confirm_boot tests | `tests/test_loxboot_confirm_boot.c` | CTest (MSVC Debug/Release) | VERIFIED | No loxboot_run yet |
| loxboot_commit_slot | `src/loxboot_core.c` | CTest (MSVC Debug/Release) | VERIFIED | No firmware CRC verification yet |
| loxboot_commit_slot tests | `tests/test_loxboot_commit_slot.c` | CTest (MSVC Debug/Release) | VERIFIED | No firmware CRC verification yet |
| loxboot_invalidate_slot | `src/loxboot_core.c` | CTest (MSVC Debug/Release) | VERIFIED | Repairs corrupt slot record when state CRC is valid |
| loxboot_invalidate_slot tests | `tests/test_loxboot_invalidate_slot.c` | CTest (MSVC Debug/Release) | VERIFIED | Repairs corrupt slot record when state CRC is valid |
| loxboot_request_slot | `src/loxboot_core.c` | CTest (MSVC + clang-cl Debug/Release) | VERIFIED | No reboot/jump behavior in v0.2.0 |
| loxboot_request_slot tests | `tests/test_loxboot_request_slot.c` | CTest (MSVC + clang-cl Debug/Release) | VERIFIED | No reboot/jump behavior in v0.2.0 |
| Boot state edge-case tests | `tests/test_loxboot_state_edges.c` | CTest (MSVC + clang-cl Debug/Release) | VERIFIED | RAM flash model only |
| GitHub Actions CI passing | GitHub Actions | n/a | NOT VERIFIED | Requires v0.2.0 |
| **v0.3.0-boot-sequence** | | | | |
| loxboot_run full sequence | `src/loxboot_core.c` lines 380–522 | CTest (6 tests) | VERIFIED | All steps 1–8 verified; test mode uses flash adapter for firmware CRC |
| loxboot_run tests | `tests/test_loxboot_boot_sequence.c` | CTest (6 tests) | VERIFIED | All scenarios passing: valid slot, PENDING→ACTIVE, CRC fail fallback, no valid slot, state corrupt, boot_attempts counter |
| Rollback implementation | `src/loxboot_core.c` lines 347–378 | CTest (3 tests) | VERIFIED | Marks active ROLLBACK, promotes fallback, resets boot_reason |
| Rollback tests | `tests/test_loxboot_rollback.c` | CTest (3 tests) | VERIFIED | Crash loop rollback, no fallback error, CRC fail trigger |
| Crash loop tests | `tests/test_loxboot_crash_loop.c` | CTest (4 tests) | VERIFIED | Counter increments, threshold triggers rollback, confirm resets, persistence |
| Dual-copy corruption recovery | `src/loxboot_state.c` lines 106–132 | CTest via test_loxboot_state_edges.c | VERIFIED | Read primary or backup; restore primary from backup if corrupt |
| Test jump hook | `src/loxboot_core.c` lines 310–312, 526–531 | CTest (all new tests) | VERIFIED | Intercepted via g_jump_hook when LOXBOOT_TEST_HOOKS=1 |
| **v0.4.0-uart** | | | | |
| UART frame encode/decode | `ports/uart/loxboot_uart.c` | n/a | NOT IMPLEMENTED | v0.4.0 |
| loxboot_crc16 | `ports/uart/loxboot_uart.c` | n/a | NOT IMPLEMENTED | v0.4.0 |
| UART session: CMD_HELLO | `ports/uart/loxboot_uart.c` | n/a | NOT IMPLEMENTED | v0.4.0 |
| UART session: CMD_WRITE | `ports/uart/loxboot_uart.c` | n/a | NOT IMPLEMENTED | v0.4.0 |
| UART session: CMD_COMMIT | `ports/uart/loxboot_uart.c` | n/a | NOT IMPLEMENTED | v0.4.0 |
| UART session: CMD_ABORT | `ports/uart/loxboot_uart.c` | n/a | NOT IMPLEMENTED | v0.4.0 |
| UART frame tests | `tests/test_loxboot_uart_frame.c` | n/a | NOT IMPLEMENTED | v0.4.0 |
| UART receive tests | `tests/test_loxboot_uart_receive.c` | n/a | NOT IMPLEMENTED | v0.4.0 |
| **v0.5.0-stm32** | | | | |
| STM32 flash adapter | `adapters/stm32/loxboot_flash_stm32.c` | n/a | NOT IMPLEMENTED | v0.5.0 |
| STM32 flash adapter hardware test | Hardware | n/a | NOT VERIFIED | Hardware-only |
| **v0.6.0-esp32** | | | | |
| ESP32 flash adapter | `adapters/esp32/loxboot_flash_esp32.c` | n/a | NOT IMPLEMENTED | v0.6.0 |
| ESP32 flash adapter hardware test | Hardware | n/a | NOT VERIFIED | Hardware-only |
| **Never (out of scope)** | | | | |
| Secure boot / firmware signing | n/a | n/a | OUT OF SCOPE | Not planned |
| Bootloader self-update | n/a | n/a | OUT OF SCOPE | Not planned |
| File system / wear leveling | n/a | n/a | OUT OF SCOPE | Not planned |
