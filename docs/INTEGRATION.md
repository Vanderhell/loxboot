# loxboot - Integration Notes

This document records the integration points that exist in this repository.

## Standalone use

loxboot has zero required dependencies on other libraries.
It can be used in any C99 bare-metal project with no other changes.

## Application-side integration

The application can read the boot reason after startup and react to rollback or crash-loop recovery.

```c
loxboot_boot_reason_t reason = loxboot_get_boot_reason(&loxboot_ctx);
if (reason == LOXBOOT_REASON_ROLLBACK) {
    /* Handle rollback here. */
}
```

The application can log the boot reason and active slot with its own logger.

```c
loxboot_boot_reason_t reason = loxboot_get_boot_reason(&loxboot_ctx);
loxboot_slot_id_t     slot   = loxboot_ctx.active_slot;
/* Log reason and active slot with the application's logger. */
```

The application can keep its own boot-loop counter alongside loxboot's boot state counter.
The counters are independent.

## Dependency matrix

| loxboot feature | Integration point | Dependency type |
|---|---|---|
| Boot reason handling | application logger | Optional, app-side only |
| Crash correlation | application state | Optional, app-side only |
| App-layer loop detection | application counter | Optional, independent |
| CRC32 | loxboot internal | No external dep |
| Flash | loxboot_flash_adapter_t | Platform-provided |
| UART transport | loxboot_transport_adapter_t | Platform-provided |

## Firmware transfer

Firmware images are transferred as raw binary.
Compression before transfer is the responsibility of the host tool, not loxboot.
