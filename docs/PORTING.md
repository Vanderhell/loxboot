# loxboot — Porting Guide

How to write a flash adapter and integrate loxboot into a new platform.

---

## What a port consists of

A loxboot port for a new platform requires:

1. **Flash adapter** (`loxboot_flash_adapter_t`) — read, write, erase
2. **Clock adapter** (`loxboot_clock_adapter_t`) — millisecond timestamp
3. **Transport adapter** (`loxboot_transport_adapter_t`) — if UART update is needed
4. **HAL** (`loxboot_hal_t`) — fatal handler, never returns
5. **Platform config** (`loxboot_platform_t`) — flash addresses

loxboot core has no other platform requirements.

---

## Minimal port example (bare Cortex-M, no HAL)

```c
#include "loxboot/loxboot.h"

/* -----------------------------------------------------------------------
 * Flash adapter — STM32 internal flash (bare registers, no HAL)
 * -------------------------------------------------------------------- */

static loxboot_err_t my_flash_read(void *ctx, uint32_t addr, uint8_t *buf, size_t len) {
    (void)ctx;
    /* Internal flash is memory-mapped — direct pointer read */
    const uint8_t *src = (const uint8_t *)addr;
    for (size_t i = 0; i < len; i++) buf[i] = src[i];
    return LOXBOOT_OK;
}

static loxboot_err_t my_flash_write(void *ctx, uint32_t addr, const uint8_t *buf, size_t len) {
    (void)ctx;
    /* Platform-specific: unlock flash, write word by word, lock */
    /* ... */
    return LOXBOOT_OK;
}

static loxboot_err_t my_flash_erase(void *ctx, uint32_t addr, size_t len) {
    (void)ctx;
    /* Platform-specific: erase sector(s) covering addr..addr+len */
    /* ... */
    return LOXBOOT_OK;
}

/* -----------------------------------------------------------------------
 * Clock adapter — SysTick-based millisecond counter
 * -------------------------------------------------------------------- */

static volatile uint32_t g_tick_ms = 0;

void SysTick_Handler(void) { g_tick_ms++; }

static uint32_t my_clock_now_ms(void *ctx) {
    (void)ctx;
    return g_tick_ms;
}

/* -----------------------------------------------------------------------
 * HAL — reset on fatal
 * -------------------------------------------------------------------- */

static void my_fatal(void *ctx, loxboot_err_t reason) {
    (void)ctx;
    (void)reason;
    /* Reset MCU */
    /* SCB->AIRCR = 0x05FA0004; */
    while (1) {} /* Never returns */
}

/* -----------------------------------------------------------------------
 * UART transport adapter (optional)
 * -------------------------------------------------------------------- */

static loxboot_err_t my_uart_read_byte(void *ctx, uint8_t *out, uint32_t timeout_ms) {
    (void)ctx;
    uint32_t start = g_tick_ms;
    while ((g_tick_ms - start) < timeout_ms) {
        if (USART_RX_NOT_EMPTY()) {
            *out = USART_READ_BYTE();
            return LOXBOOT_OK;
        }
    }
    return LOXBOOT_ERR_TIMEOUT;
}

static loxboot_err_t my_uart_write_byte(void *ctx, uint8_t b) {
    (void)ctx;
    while (!USART_TX_READY()) {}
    USART_WRITE_BYTE(b);
    return LOXBOOT_OK;
}

static loxboot_err_t my_uart_flush(void *ctx) {
    (void)ctx;
    while (!USART_TX_COMPLETE()) {}
    return LOXBOOT_OK;
}

/* -----------------------------------------------------------------------
 * Main bootloader entry point
 * -------------------------------------------------------------------- */

void bootloader_main(void) {
    /* System init (clocks, SysTick, UART) — platform-specific */
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000);
    USART1_Init(115200);

    /* Build loxboot context */
    loxboot_t ctx = {0};

    ctx.flash.read  = my_flash_read;
    ctx.flash.write = my_flash_write;
    ctx.flash.erase = my_flash_erase;

    ctx.clock.now_ms = my_clock_now_ms;

    ctx.hal.on_fatal = my_fatal;

    ctx.transport.read_byte  = my_uart_read_byte;
    ctx.transport.write_byte = my_uart_write_byte;
    ctx.transport.flush      = my_uart_flush;

    ctx.platform.boot_state_primary_base = 0x08004000;
    ctx.platform.boot_state_backup_base  = 0x08008000;
    ctx.platform.slot_a_base             = 0x08020000;
    ctx.platform.slot_b_base             = 0x08060000;
    ctx.platform.slot_size               = 0x00040000;

    /* Init and run — never returns */
    loxboot_err_t err = loxboot_init(&ctx);
    if (err != LOXBOOT_OK) {
        my_fatal(NULL, err);
    }

    loxboot_run(&ctx); /* Never returns */
}
```

---

## Flash adapter rules

1. `read` must work for any byte-aligned address and length within slot or boot state region.
2. `write` assumes the target region is already erased (0xFF). loxboot does not erase before writing firmware — the transport layer is responsible for erasing before writing.
3. `erase` must accept any address/length within platform regions. If the platform requires sector alignment, the adapter is responsible for rounding up.
4. All three must be synchronous (blocking). loxboot has no async model.
5. Return exact `loxboot_err_t` codes — do not map all errors to `LOXBOOT_ERR_FLASH_READ`.

---

## Boot state region sizing

The boot state region must be large enough for two copies of `loxboot_state_t`:

```c
/* Minimum boot state region per copy */
size_t min_size = sizeof(loxboot_state_t);  /* typically ~60 bytes */

/* Total flash reserved for boot state */
size_t total = min_size * 2;  /* primary + backup */
```

On flash with large sector sizes (e.g. STM32F4 with 16KB sectors), one sector per
copy is typical even though `loxboot_state_t` is much smaller than 16KB.
The extra space is wasted but unavoidable due to erase granularity.

---

## Application side: confirming boot

Add this to application startup, after self-test:

```c
#include "loxboot/loxboot.h"

extern loxboot_t g_loxboot_ctx;  /* shared between bootloader and app */

void app_startup(void) {
    /* ... peripheral init, self-test ... */

    loxboot_confirm_boot(&g_loxboot_ctx);

    /* ... rest of app ... */
}
```

If the application and bootloader share flash (typical), `g_loxboot_ctx` must be
in a RAM section that survives the jump (e.g. `.noinit` or `.persistent`).

Alternatively, the application can re-initialize `loxboot_t` with the same platform
config and call `loxboot_confirm_boot()` — loxboot will re-read state from flash.

---

## Linker considerations (Cortex-M)

The bootloader must be linked to start at the beginning of flash.
The application must be linked to start at `slot_a_base` (or `slot_b_base`).

Application linker script must set `VTOR` (Vector Table Offset Register):
```c
/* In application startup, before enabling interrupts */
SCB->VTOR = SLOT_A_BASE;
```

The bootloader does not set VTOR — it reads the vector table from the slot base
to find the stack pointer and reset handler.

---

## Testing your adapter

Use the RAM adapter test pattern from `AGENT_BRIEF.md` to verify your adapter
contract before running on hardware:

```c
/* Swap your real adapter for the RAM adapter in tests */
ctx.flash.read  = ram_flash_read;
ctx.flash.write = ram_flash_write;
ctx.flash.erase = ram_flash_erase;
ctx.flash.ctx   = g_flash_buffer;
```

All loxboot CTest tests use this pattern.
