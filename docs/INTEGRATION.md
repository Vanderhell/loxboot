# loxboot — Ecosystem Integration Guide

How loxboot integrates with the Vanderhell embedded ecosystem.

---

## Standalone use (no ecosystem dependencies)

loxboot has zero required dependencies on other Vanderhell libraries.
It can be used in any C99 bare-metal project with no other changes.

---

## Optional ecosystem integrations

### panicdump

`panicdump` captures Cortex-M fault information (registers, stack) and stores it
in a memory region that survives reboot (`.noinit`).

**Integration point:** After `loxboot_run()` jumps to the app, the app can check
if a panic dump exists from the previous boot and correlate it with `loxboot_get_boot_reason()`.

```c
// In application startup:
loxboot_boot_reason_t reason = loxboot_get_boot_reason(&loxboot_ctx);

if (reason == LOXBOOT_REASON_ROLLBACK) {
    panicdump_header_t *dump = panicdump_get();
    if (dump && panicdump_is_valid(dump)) {
        // Log: "Rolled back after crash: PC=0x%08X LR=0x%08X"
        nvlog_write(...);
        panicdump_clear();
    }
}
```

loxboot does not call panicdump directly. No dependency.

---

### nvlog

`nvlog` provides persistent flash-backed logging.

**Integration point:** Log boot events after application starts.

```c
loxboot_boot_reason_t reason = loxboot_get_boot_reason(&loxboot_ctx);
loxboot_slot_id_t     slot   = loxboot_ctx.active_slot;

nvlog_write_fmt(NVLOG_INFO, "boot: reason=%d slot=%d", (int)reason, (int)slot);
```

loxboot does not call nvlog directly. No dependency.

---

### microboot

`microboot` provides application-layer boot loop detection via a counter in
persistent RAM or flash. It operates independently from loxboot's bootloader-layer
crash loop detection.

**Relationship:**
- loxboot counts boot attempts in the boot state region (flash-backed)
- microboot counts boot attempts from the application side
- Both can coexist — they count the same event from different layers
- If both are present, the application should call `loxboot_confirm_boot()` AND
  reset the microboot counter at the same point in startup

**No direct dependency.** They operate on separate counters.

---

### loxruntime (future — vh_boot facade)

When loxruntime gains a `vh_boot` facade (future milestone), loxboot becomes
the default backend. The integration is a thin adapter:

```c
// Future: loxruntime/adapters/vh_boot_loxboot.c
// Not part of loxboot itself.

#include "loxruntime/vh_boot.h"
#include "loxboot/loxboot.h"

static vh_err_t adapter_confirm(vh_boot_t *vb) {
    loxboot_t *lb = (loxboot_t *)vb->impl;
    return loxboot_confirm_boot(lb) == LOXBOOT_OK ? VH_OK : VH_ERR_INVALID_STATE;
}

static vh_err_t adapter_get_reason(vh_boot_t *vb, vh_boot_reason_t *out) {
    loxboot_t *lb = (loxboot_t *)vb->impl;
    *out = (vh_boot_reason_t)loxboot_get_boot_reason(lb);
    return VH_OK;
}
```

loxboot itself never includes loxruntime headers.

---

## Dependency matrix

| loxboot feature | Ecosystem library | Dependency type |
|---|---|---|
| Boot reason logging | nvlog | Optional, app-side only |
| Crash correlation | panicdump | Optional, app-side only |
| App-layer loop detection | microboot | Optional, independent |
| loxruntime vh_boot facade | loxruntime | Future, adapter-only |
| CRC32 | loxboot internal | No external dep |
| Flash | loxboot_flash_adapter_t | Platform-provided |
| UART transport | loxboot_transport_adapter_t | Platform-provided |

---

## microcodec / loxc

Not integrated with loxboot. Firmware images are transferred as raw binary.
Compression of the image before transfer is the responsibility of the host tool, not loxboot.
