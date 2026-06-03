#ifndef LOXBOOT_FLASH_STM32_H
#define LOXBOOT_FLASH_STM32_H

#include <stdint.h>
#include <stddef.h>

#include "loxboot/loxboot.h"

/* STM32 internal flash adapter context (no extra state needed) */
typedef struct {
    uint8_t _reserved;
} loxboot_stm32_flash_ctx_t;

/* Initialize STM32 flash adapter
 *
 * Fills out->read/write/erase/ctx with STM32 internal flash implementation.
 * Uses STM32 HAL for write/erase operations.
 * Read is memory-mapped (direct pointer access).
 *
 * Parameters:
 *   out  — pointer to loxboot_flash_adapter_t to initialize
 *   ctx  — pointer to loxboot_stm32_flash_ctx_t (may be zeroed)
 *
 * Returns: void (always succeeds)
 */
void loxboot_stm32_flash_adapter_init(
    loxboot_flash_adapter_t *out,
    loxboot_stm32_flash_ctx_t *ctx);

#endif /* LOXBOOT_FLASH_STM32_H */
