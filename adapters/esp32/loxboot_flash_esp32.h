#ifndef LOXBOOT_FLASH_ESP32_H
#define LOXBOOT_FLASH_ESP32_H

#include <stdint.h>
#include <stddef.h>

#include "loxboot/loxboot.h"

/* Forward declaration — user provides via ESP-IDF esp_partition.h */
typedef struct esp_partition_t esp_partition_t;

/* ESP32 flash adapter context */
typedef struct {
    const esp_partition_t *partition;  /* Partition handle from esp_partition API */
} loxboot_esp32_flash_ctx_t;

/* Initialize ESP32 flash adapter
 *
 * Fills out->read/write/erase/ctx with ESP32 flash implementation.
 * Uses ESP-IDF esp_partition API for all flash operations.
 *
 * Parameters:
 *   out       — pointer to loxboot_flash_adapter_t to initialize
 *   ctx       — pointer to loxboot_esp32_flash_ctx_t with partition set
 *
 * Returns: void (always succeeds)
 *
 * The caller must initialize ctx->partition via esp_partition_find_first()
 * before calling this function.
 */
void loxboot_esp32_flash_adapter_init(
    loxboot_flash_adapter_t *out,
    loxboot_esp32_flash_ctx_t *ctx);

#endif /* LOXBOOT_FLASH_ESP32_H */
