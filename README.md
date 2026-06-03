# loxboot

Minimal, platform-agnostic bootloader core for bare-metal MCUs.

Part of the [Vanderhell](https://github.com/Vanderhell) embedded ecosystem.

---

## What it is

loxboot is a **C99, zero-heap, platform-agnostic bootloader core**.

It handles everything that must run before the main application:

- CRC32 implementation
- Boot-state read/write helpers (dual-copy)
- Slot metadata state changes (commit/invalidate/confirm/request)
- Boot reason reporting (in-memory)
- Firmware update receive over UART (v0.4.0+; not implemented in v0.2.0)
- Full boot sequence + jump (v0.3.0+; not implemented in v0.2.0)

It does **not** implement flash drivers, UART peripheral registers, or any hardware-specific code.
All hardware access is injected via adapter structs (function pointers).

---

## Philosophy

Same as the rest of the Vanderhell ecosystem:

- **Zero heap.** No malloc, no calloc, no dynamic allocation anywhere.
- **Zero dependencies.** No RTOS, no HAL, no external libraries.
- **Platform-agnostic core.** Flash, clock, and transport are injected adapters.
- **Adapter-first.** Every hardware operation is behind a function pointer.
- **Specs before code.** Public API is a contract. Backends implement the contract.
- **Ecosystem-aware.** Integrates naturally with `panicdump`, `nvlog`, `microboot`, and `loxruntime` — but requires none of them.

---

## Where it fits

```
┌─────────────────────────────────────────────────────┐
│                   Main Application                  │
├─────────────────────────────────────────────────────┤
│   loxruntime vh_boot facade (future integration)    │
├──────────────┬──────────────────────────────────────┤
│              │  panicdump  nvlog  microboot          │
│   loxboot    │  (optional ecosystem integrations)   │
│   (this)     │  no direct dependency                │
├──────────────┴──────────────────────────────────────┤
│   Flash adapter │ Clock adapter │ Transport adapter  │
├─────────────────────────────────────────────────────┤
│                       Hardware                      │
└─────────────────────────────────────────────────────┘
```

---

## Quick start

```c
#include "loxboot/loxboot.h"

/* 1. Fill context */
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

/* 2. Init */
loxboot_err_t err = loxboot_init(&ctx);
if (err != LOXBOOT_OK) { my_fatal(NULL, err); }

/* 3. Run — never returns */
/* v0.2.0-core note: loxboot_run() is a stub and returns LOXBOOT_ERR_INVALID_STATE. */
err = loxboot_run(&ctx);
(void)err;

/* ------------------------------------------------------------------ */
/* In your application, after successful startup: */
loxboot_confirm_boot(&ctx);  /* resets crash counter */
```

---

## Feature set by version

| Feature | v0.1.0 | v0.2.0 | v0.3.0 | v0.4.0 | v0.5.0 | v0.6.0 |
|---|---|---|---|---|---|---|
| Public API + full spec | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| CRC32, init, slot control | spec | ✅ | ✅ | ✅ | ✅ | ✅ |
| loxboot_run, rollback, crash loop | spec | — | ✅ | ✅ | ✅ | ✅ |
| UART transport port | spec | — | — | ✅ | ✅ | ✅ |
| STM32 flash adapter | — | — | — | — | ✅ | ✅ |
| ESP32 flash adapter | — | — | — | — | — | ✅ |

---

## Documentation

- [docs/SPEC.md](docs/SPEC.md) — Full specification (all versions)
- [docs/PORTING.md](docs/PORTING.md) — How to write a flash adapter
- [docs/MEMORY_LAYOUT.md](docs/MEMORY_LAYOUT.md) — Memory layout reference (STM32, ESP32, generic)
- [docs/INTEGRATION.md](docs/INTEGRATION.md) — Ecosystem integration guide
- [AGENT_BRIEF.md](AGENT_BRIEF.md) — Implementation brief for automated agents
- [EVIDENCE_MATRIX.md](EVIDENCE_MATRIX.md) — Verified claims per version
- [PROJECT_STATE.md](PROJECT_STATE.md) — Current status and roadmap

---

## Build

```sh
cmake -S . -B build -DLOXBOOT_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Optional:
```sh
-DLOXBOOT_BUILD_UART_PORT=ON     # Include UART transport port
-DLOXBOOT_MAX_BOOT_ATTEMPTS=3    # Rollback threshold (default: 3)
-DLOXBOOT_UART_LISTEN_MS=3000    # UART listen window in ms (default: 3000)
```

---

## Current status

`v0.2.0-core candidate` — CRC32 + init + boot-state R/W + slot control + tests (locally verified on MSVC and clang-cl).

See [EVIDENCE_MATRIX.md](EVIDENCE_MATRIX.md) and [PROJECT_STATE.md](PROJECT_STATE.md).

---

## License

MIT
