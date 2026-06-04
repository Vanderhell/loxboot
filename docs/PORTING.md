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

## STM32 internal flash adapter (v0.5.0+)

loxboot includes a ready-made adapter for STM32 internal flash using the STM32 HAL.

### Setup

1. **Include the adapter header:**
   ```c
   #include "loxboot_flash_stm32.h"
   ```

2. **Provide STM32 HAL via include path:**
   The adapter includes a single header `stm32_hal.h` which your build must provide.
   This allows the adapter to work with any STM32 variant (F0, F1, F4, H7, etc).

   In your CMakeLists.txt or build system:
   ```cmake
   include_directories(/path/to/stm32_hal_headers)
   target_compile_definitions(your_target PRIVATE LOXBOOT_BUILD_STM32_ADAPTER=ON)
   ```

   The `stm32_hal.h` file should be a wrapper that includes the correct HAL header:
   ```c
   /* stm32_hal.h — user-provided wrapper */
   #ifndef STM32_HAL_H
   #define STM32_HAL_H

   #include "stm32f4xx_hal.h"  /* or stm32h7xx_hal.h, stm32f0xx_hal.h, etc */

   #endif
   ```

3. **Initialize the adapter in your bootloader:**
   ```c
   #include "loxboot_flash_stm32.h"

   void bootloader_main(void) {
       loxboot_t ctx = {0};
       loxboot_stm32_flash_ctx_t flash_ctx = {0};

       /* Initialize STM32 flash adapter */
       loxboot_stm32_flash_adapter_init(&ctx.flash, &flash_ctx);

       /* Set remaining context fields (clock, hal, platform, etc) */
       ctx.clock.now_ms = my_clock_now_ms;
       ctx.hal.on_fatal = my_fatal;
       /* ... */

       /* Initialize and run */
       loxboot_init(&ctx);
       loxboot_run(&ctx);  /* Never returns */
   }
   ```

### How the adapter works

- **read**: Direct memory-mapped read from flash address (no HAL call needed)
- **write**: Uses `HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ...)` to write 8-byte chunks.
  Returns `LOXBOOT_ERR_FLASH_WRITE` on HAL error.
- **erase**: Uses `HAL_FLASHEx_Erase()` to erase pages.
  Requires page-aligned address and length. Returns `LOXBOOT_ERR_INVALID_ARG` if not aligned.

### Hardware verification

The adapter is compiled with `-Wall -Wextra -Wpedantic -Werror` on your target toolchain.
You must:
1. Configure your platform's `FLASH_PAGE_SIZE` and flash bank via the HAL.
2. Test the adapter on actual hardware before deployment.

---

## ESP32 flash adapter (v0.6.0+)

loxboot includes a ready-made adapter for ESP32 internal flash using the ESP-IDF esp_partition API.

### Setup

1. **Include the adapter header:**
   ```c
   #include "loxboot_flash_esp32.h"
   ```

2. **Find the target partition:**
   Use `esp_partition_find_first()` to locate the partition that will hold loxboot state and firmware:
   ```c
   #include "esp_partition.h"
   #include "loxboot_flash_esp32.h"

   const esp_partition_t *partition = esp_partition_find_first(
       ESP_PARTITION_TYPE_DATA,
       ESP_PARTITION_SUBTYPE_DATA_OTA,
       "boot_app"  /* Partition label — configure in partitions.csv */
   );
   if (partition == NULL) {
       /* Partition not found — check partitions.csv */
       return;
   }
   ```

3. **Initialize the adapter in your bootloader:**
   ```c
   void app_main(void) {
       /* Initialize loxboot context */
       loxboot_t ctx = {0};
       loxboot_esp32_flash_ctx_t flash_ctx = {0};

       /* Find partition and initialize flash adapter */
       flash_ctx.partition = esp_partition_find_first(
           ESP_PARTITION_TYPE_DATA,
           ESP_PARTITION_SUBTYPE_DATA_OTA,
           "boot_app");
       if (flash_ctx.partition == NULL) {
           ESP_LOGE(TAG, "Boot partition not found");
           return;
       }

       loxboot_esp32_flash_adapter_init(&ctx.flash, &flash_ctx);

       /* Set remaining context fields (clock, hal, transport, platform) */
       ctx.clock.now_ms = my_clock_now_ms;
       ctx.hal.on_fatal = my_fatal;
       /* ... */

       /* Initialize and run */
       loxboot_init(&ctx);
       loxboot_run(&ctx);  /* Never returns on success */
   }
   ```

### How the adapter works

- **read**: Direct call to `esp_partition_read(partition, addr, buf, len)`
- **write**: Direct call to `esp_partition_write(partition, addr, buf, len)`
  Each call handles alignment and caching internally.
  Returns `LOXBOOT_ERR_FLASH_WRITE` if any operation fails.
- **erase**: Direct call to `esp_partition_erase_range(partition, addr, len)`
  Must be sector-aligned (typically 4KB on ESP32).

### Partition configuration

Edit `partitions.csv` to define regions for boot state and firmware slots:

```
# Name,     Type,  SubType, Offset,   Size,      Flags
nvs,        data,  nvs,     0x9000,   0x6000,    
otadata,    data,  ota,     0xf000,   0x2000,    
boot_app,   data,  ota,     0x11000,  0x100000,  
```

The partition named `boot_app` will be mounted at offset `0x11000` with size `0x100000` (1 MB).
Set `loxboot_platform_t` fields accordingly (in terms of offset within the partition, from 0).

### Hardware verification

The adapter is compiled with `-Wall -Wextra` on xtensa-esp32-elf-gcc.
Test the adapter on actual ESP32 hardware before deployment.

---

## Flash adapter rules

1. `read` must work for any byte-aligned address and length within slot or boot state region.
2. `write` assumes the target region is already erased (0xFF). loxboot does not erase before writing firmware — the transport layer is responsible for erasing before writing.
3. `erase` must accept any address/length within platform regions. The adapter is responsible for rounding up to sector/page boundaries. loxboot may pass sub-sector sizes (e.g. 60 bytes for boot state). The caller guarantees that the memory layout places boot state copies in isolated sectors so the rounding is safe.
4. All three must be synchronous (blocking). loxboot has no async model.
5. Return exact `loxboot_err_t` codes — do not map all errors to `LOXBOOT_ERR_FLASH_READ`.

---

## Boot state region sizing and isolation

**Rule: each boot state copy must occupy its own erase sector exclusively.**

loxboot calls `flash.erase(addr, sizeof(loxboot_state_t))` — approximately 60 bytes.
The adapter must round this up to the platform's erase granularity.
This means the erase will cover an entire sector, erasing everything in it.

**Any data that shares a sector with a boot state copy will be destroyed on every boot state update.**

This includes: application code, slot firmware, configuration, NVS, or any other data.

```
CORRECT layout (STM32, 2KB pages):
  0x08004000 — boot_state_primary  [2KB page, nothing else]
  0x08004800 — boot_state_backup   [2KB page, nothing else]
  0x08005000 — slot A or app start

WRONG layout (data corruption):
  0x08004000 — boot_state_primary  [shares page with config at 0x08004100]
  ^^ CONFIG AT 0x08004100 WILL BE ERASED WHEN BOOT STATE IS UPDATED ^^
```

For platforms with large sectors (STM32F4: 16KB / 128KB sectors), allocate one full sector per boot state copy. The wasted space is unavoidable.

For ESP32, use dedicated partitions (4KB minimum, one per copy):
```csv
loxstate, data, 0x40,  0x10000, 0x1000,
loxbkup,  data, 0x41,  0x11000, 0x1000,
```

**Minimum sizes by platform:**

| Platform | Sector size | Minimum per copy |
|----------|-------------|------------------|
| STM32F1/F3 | 1–2 KB | 1 sector (1–2 KB) |
| STM32F4 | 16 KB (small) / 128 KB (large) | 1 sector |
| STM32L4/G4 | 2–4 KB | 1 sector |
| ESP32 | 4 KB | 1 sector (4 KB) |
| ESP32-S3 | 4 KB | 1 sector (4 KB) |

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
