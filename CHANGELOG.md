# Changelog — loxboot

## v0.6.0 (Current)

### Critical Fixes
- **GCC compatibility**: Fixed pointer casting using `uintptr_t` intermediate cast (addresses `-Werror=int-to-pointer-cast`)
- **ARM architecture detection**: Changed from compiler-macro detection to ARM architecture detection (`__arm__`, `__thumb__`, `__ARM_ARCH`) to prevent ARM MSR instruction compilation on x86 host
- **C99 pedantic compliance**: Fixed label-before-declaration error by wrapping boot_retry in block scope
- **CRC16-CCITT-FALSE**: Fixed algorithm (was using wrong byte-indexing; now correctly implements standard CRC16-CCITT)
- **Version consistency**: Unified version across loxboot_version.h and loxboot.h (0.6.0)

### Features
- v0.6.0 baseline: Dual-copy boot state, 8-step boot sequence, UART transport, STM32 & ESP32 adapters
- Enhanced UART tests: Added command constant validation and session state tests
- ARM cross-compile support: New CI matrix entry validates arm-none-eabi-gcc builds

### Test Results
- All 13 tests passing (11 core + 2 UART extensions)
- CI validates 5-target matrix (Ubuntu GCC/Clang, arm-none-eabi, Windows MSVC/Clang-CL)
- CRC16 known-vector test now passes: `crc16("123456789") = 0x29B1` ✓

### Documentation Updates
- README status clarified: prototype/reference implementation, production-ready for ARM Cortex-M
- Removed outdated CRC16 bug note (now fixed)
- Updated CI matrix documentation to reflect 5 targets

## v0.5.0

- STM32 HAL-based flash adapter (loxboot_flash_stm32.c)
- Hardware validation on STM32 platforms

## v0.4.0

- UART transport protocol (frame-based, CRC16)
- loxboot_uart_run_session() public API
- Transport adapter interface (read_byte/write_byte/flush)

## v0.3.0

- 8-step boot sequence with crash loop detection
- Automatic rollback on excessive boot attempts
- loxboot_run() public API

## v0.2.0

- Dual-copy boot state management
- CRC32 corruption recovery
- Slot control API (commit, invalidate, request, confirm)
- State validation with partial recovery

## v0.1.0

- Public API specification (SPEC.md)
- Core data structures (loxboot_state_t, loxboot_slot_t)
- Reference implementation (non-functional prototype)
