# loxboot - Bootloader Core

[![Version](https://img.shields.io/github/v/tag/Vanderhell/loxboot?label=version&color=blue&style=flat-square)](https://github.com/Vanderhell/loxboot/tags)
[![License: MIT](https://img.shields.io/badge/license-MIT-green?style=flat-square)](LICENSE)
![C99](https://img.shields.io/badge/standard-C99-00599C?style=flat-square&logo=c)
![Tests](https://img.shields.io/badge/tests-15%20passing-brightgreen?style=flat-square)
![No heap](https://img.shields.io/badge/heap-none%20%2F%20zero%20deps-success?style=flat-square)
![Platforms](https://img.shields.io/badge/platforms-STM32%20%7C%20ESP32--S3-555?style=flat-square)
![Status](https://img.shields.io/badge/status-not%20production%20ready-orange?style=flat-square)

Minimal, adapter-based bootloader core for bare-metal MCUs. The repository includes host-side tests, UART protocol tests, STM32 and ESP32 adapter code, a Python E2E simulator, and an ESP32-S3 OTA hardware harness.

Local verification in this workspace:
- Host automated tests: `15/15` CTest binaries passing with `-DLOXBOOT_BUILD_UART_PORT=ON`
- GitHub Actions: `PREPARED / NOT RUN IN THIS TASK`
- GitHub Release: `workflow present / release not verified in this task`
- ESP32-S3 hardware: `test harness present / hardware evidence required`
- STM32 hardware: `adapter present / hardware evidence required`
- Power-loss tests: `plan present / not verified in this task`
- Firmware signing: `not implemented`

## Status

**NOT PRODUCTION READY**

The host-core portion of the repository is verified locally, but platform hardening remains incomplete. The current blockers are hardware validation, power-loss evidence, and firmware authentication.

Platform adapters still require evidence for:
- Flash erase granularity handling on target hardware
- UART slot erase behavior during update
- Firmware CRC verification via flash adapters
- Power-loss and corruption recovery
- Hardware jump and reboot flow
- Firmware signing or authentication

## Overview

loxboot is a C99, zero-heap bootloader core that provides:

- Boot state management with dual-copy CRC32 recovery
- Slot control for firmware A/B workflows
- Boot sequencing with crash-loop detection and rollback
- UART transport for in-field firmware updates
- STM32 and ESP32 adapter layers
- Python-based E2E simulator coverage in CTest
- ESP32-S3 OTA hardware harness for later evidence collection

## Architecture

### Core
- `src/loxboot_core.c` - boot sequence, slot control, state management
- `src/loxboot_state.c` - state read/write/validate with dual-copy recovery
- `src/loxboot_crc32.c` - CRC32/IEEE implementation

### Adapters
- `adapters/stm32/loxboot_flash_stm32.c` - STM32 flash adapter
- `adapters/esp32/loxboot_flash_esp32.c` - ESP32 esp_partition adapter

### Transport
- `ports/uart/loxboot_uart.c` - UART frame protocol

## Quick Start

```c
#include "loxboot/loxboot.h"

loxboot_t ctx = {0};

ctx.flash.read  = my_flash_read;
ctx.flash.write = my_flash_write;
ctx.flash.erase = my_flash_erase;
ctx.clock.now_ms = my_clock_now_ms;
ctx.hal.on_fatal = my_fatal;

ctx.platform.boot_state_primary_base = 0x08004000;
ctx.platform.boot_state_backup_base  = 0x08008000;
ctx.platform.slot_a_base             = 0x08020000;
ctx.platform.slot_b_base             = 0x08060000;
ctx.platform.slot_size               = 0x00040000;

if (loxboot_init(&ctx) != LOXBOOT_OK) {
    ctx.hal.on_fatal(&ctx, LOXBOOT_ERR_INVALID_STATE);
}

loxboot_run(&ctx);
```

In the application:

```c
loxboot_confirm_boot(&ctx);
```

## Feature Matrix

| Feature | v0.1.0 | v0.2.0 | v0.3.0 | v0.4.0 | v0.5.0 | v0.6.0 | v0.7.0 |
|---|---|---|---|---|---|---|---|
| Public API + spec | spec | yes | yes | yes | yes | yes | yes |
| CRC32, init, slot control | spec | yes | yes | yes | yes | yes | yes |
| Boot sequence + rollback + crash detection | spec | - | yes | yes | yes | yes | yes |
| UART transport (CRC16, frame protocol) | spec | - | - | yes | yes | yes | yes |
| STM32 flash adapter | - | - | - | - | yes | yes | yes |
| ESP32 flash adapter | - | - | - | - | - | yes | yes |
| Python E2E simulator | - | - | - | - | - | - | yes |
| ESP32-S3 OTA harness | - | - | - | - | - | - | yes |

## Build

```bash
cmake -S . -B build
cmake -S . -B build -DLOXBOOT_BUILD_UART_PORT=ON
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

Optional flags:

```bash
-DLOXBOOT_BUILD_UART_PORT=ON
-DLOXBOOT_BUILD_STM32_ADAPTER=ON
-DLOXBOOT_BUILD_ESP32_ADAPTER=ON
-DLOXBOOT_MAX_BOOT_ATTEMPTS=3
-DLOXBOOT_UART_LISTEN_MS=3000
```

## Test Coverage

Local test coverage in this workspace:
- Boot sequence
- State management
- UART frame and session handling
- Slot operations
- Init, CRC, and rollback
- ESP32 platform stub tests
- Python E2E simulator

Status:
- GitHub Actions: `PREPARED / NOT RUN IN THIS TASK`
- Hardware adapters: `evidence required`
- ESP32-S3 hardware OTA: `harness present, not verified in this task`

## Documentation

| Document | Purpose |
|---|---|
| [docs/SPEC.md](docs/SPEC.md) | Technical specification |
| [docs/PORTING.md](docs/PORTING.md) | Adapter guidance |
| [docs/MEMORY_LAYOUT.md](docs/MEMORY_LAYOUT.md) | Flash and RAM layout reference |
| [docs/INTEGRATION.md](docs/INTEGRATION.md) | Ecosystem integration |

## Constraints

- C99 only
- Zero heap
- No external runtime dependencies
- Adapter injection for hardware access
- Deterministic, synchronous behavior

## License

MIT
