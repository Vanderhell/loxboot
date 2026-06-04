# loxboot Examples

Three integration examples showing how to wire loxboot to different platforms.

## examples/generic_custom_adapter/

**Start here.** Platform-agnostic template — implement 6 functions and you're done.

```
my_flash_read()      — memcpy from flash address
my_flash_write()     — platform flash write
my_flash_erase()     — platform flash erase (round up to sector boundary)
my_uart_read_byte()  — blocking byte receive with timeout
my_uart_write_byte() — transmit one byte
my_uart_flush()      — drain TX buffer
```

## examples/stm32_uart/

STM32F4xx with STM32CubeMX HAL + USART1 for firmware update.

**Requires:** STM32CubeIDE or arm-none-eabi-gcc + STM32 HAL  
**Flash layout:** 16KB sectors (boot state), 256KB slots A/B

## examples/esp32_uart/

ESP32 with ESP-IDF v5 + partition table + UART0 for firmware update.

**Requires:** ESP-IDF v5.x, custom `partitions.csv`  
**Flash layout:** See partition table comment in `app_main.c`

---

## Minimum required adapters

| Adapter | Required | Without it |
|---------|----------|------------|
| `flash.read/write/erase` | Always | `loxboot_init()` returns error |
| `hal.on_fatal` | Always | `loxboot_init()` returns error |
| `transport.read_byte/write_byte/flush` | For UART update | No update over UART (boot-only mode) |
| `clock.now_ms` | For UART update | UART session disabled |

## Application side (confirm boot)

After your app starts successfully, call:

```c
extern loxboot_t g_loxboot;  /* shared via .noinit RAM */

void app_startup_complete(void)
{
    loxboot_confirm_boot(&g_loxboot);
}
```

Without this call, loxboot increments `boot_attempts` each boot.
After `LOXBOOT_MAX_BOOT_ATTEMPTS` (default: 3) boots, automatic rollback triggers.
