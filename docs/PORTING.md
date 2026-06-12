# loxboot Porting Guide

This document shows the adapter and layout requirements for a new platform.

## Required pieces

1. Flash adapter: `loxboot_flash_adapter_t`
2. Clock adapter: `loxboot_clock_adapter_t`
3. Transport adapter: `loxboot_transport_adapter_t` when UART update is used
4. HAL: `loxboot_hal_t`
5. Platform config: `loxboot_platform_t`

loxboot core has no other platform requirements.

## Minimal bare-metal example

```c
#include "loxboot/loxboot.h"

static loxboot_err_t my_flash_read(void *ctx, uint32_t addr, uint8_t *buf, size_t len) {
    (void)ctx;
    const uint8_t *src = (const uint8_t *)addr;
    for (size_t i = 0; i < len; i++) buf[i] = src[i];
    return LOXBOOT_OK;
}

static loxboot_err_t my_flash_write(void *ctx, uint32_t addr, const uint8_t *buf, size_t len) {
    (void)ctx;
    (void)addr;
    (void)buf;
    (void)len;
    return LOXBOOT_OK;
}

static loxboot_err_t my_flash_erase(void *ctx, uint32_t addr, size_t len) {
    (void)ctx;
    (void)addr;
    (void)len;
    return LOXBOOT_OK;
}

static uint32_t my_clock_now_ms(void *ctx) {
    (void)ctx;
    return 0;
}

static void my_fatal(void *ctx, loxboot_err_t reason) {
    (void)ctx;
    (void)reason;
    for (;;) {}
}

static loxboot_err_t my_uart_read_byte(void *ctx, uint8_t *out, uint32_t timeout_ms) {
    (void)ctx;
    (void)out;
    (void)timeout_ms;
    return LOXBOOT_ERR_TIMEOUT;
}

static loxboot_err_t my_uart_write_byte(void *ctx, uint8_t b) {
    (void)ctx;
    (void)b;
    return LOXBOOT_OK;
}

static loxboot_err_t my_uart_flush(void *ctx) {
    (void)ctx;
    return LOXBOOT_OK;
}

void bootloader_main(void) {
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

    if (loxboot_init(&ctx) != LOXBOOT_OK) {
        my_fatal(NULL, LOXBOOT_ERR_INVALID_ARG);
    }

    loxboot_run(&ctx);
}
```

## STM32 internal flash adapter

The STM32 adapter uses a wrapper header that includes the correct STM32 HAL header for the target family.

```c
#include "loxboot_flash_stm32.h"
```

The adapter uses memory-mapped reads, HAL-backed writes, and sector erase calls.
Boot state copies belong in separate erase sectors.

## ESP32 flash adapter

The ESP32 adapter uses `esp_partition_find_first()` to locate the partition that holds loxboot state and firmware.

```c
#include "esp_partition.h"
#include "loxboot_flash_esp32.h"

const esp_partition_t *partition = esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA,
    ESP_PARTITION_SUBTYPE_DATA_OTA,
    "boot_app");
```

The partition named `boot_app` is mounted at offset `0x11000` with size `0x100000` (1 MB) in the example layout.

## Adapter rules

1. `read` works for any byte-aligned address and length within a slot or boot state region.
2. `write` assumes the target region is already erased.
3. `erase` accepts any address/length within platform regions and rounds up to sector/page boundaries.
4. All three operations are synchronous.
5. Return exact `loxboot_err_t` codes.

## Boot state isolation

Each boot state copy occupies its own erase sector.
If another data item shares that sector, the erase removes it during boot-state updates.

## Application startup

The application can call `loxboot_confirm_boot()` after its own startup checks pass.
If the application and bootloader share flash, the shared context must live in a RAM section that survives the jump.

## Linker setup

The bootloader is linked at the start of flash.
The application is linked at `slot_a_base` or `slot_b_base`.
The application linker script sets `VTOR`.
