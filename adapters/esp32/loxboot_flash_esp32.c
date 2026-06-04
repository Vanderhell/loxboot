#include <stdint.h>
#include <stddef.h>

#include "loxboot/loxboot.h"
#include "loxboot_flash_esp32.h"
#include "esp_partition.h"

/* ESP32 flash — read via esp_partition API */
static loxboot_err_t esp32_flash_read(void *ctx, uint32_t addr, uint8_t *buf, size_t len)
{
    loxboot_esp32_flash_ctx_t *esp_ctx = (loxboot_esp32_flash_ctx_t *)ctx;

    if (esp_ctx == NULL || esp_ctx->partition == NULL || buf == NULL) {
        return LOXBOOT_ERR_FLASH_READ;
    }

    esp_err_t status = esp_partition_read(esp_ctx->partition, addr, buf, len);
    if (status != ESP_OK) {
        return LOXBOOT_ERR_FLASH_READ;
    }

    return LOXBOOT_OK;
}

/* ESP32 flash — write via esp_partition API */
static loxboot_err_t esp32_flash_write(void *ctx, uint32_t addr, const uint8_t *buf, size_t len)
{
    loxboot_esp32_flash_ctx_t *esp_ctx = (loxboot_esp32_flash_ctx_t *)ctx;

    if (esp_ctx == NULL || esp_ctx->partition == NULL || buf == NULL || len == 0u) {
        return LOXBOOT_ERR_FLASH_WRITE;
    }

    esp_err_t status = esp_partition_write(esp_ctx->partition, addr, buf, len);
    if (status != ESP_OK) {
        return LOXBOOT_ERR_FLASH_WRITE;
    }

    return LOXBOOT_OK;
}

/* ESP32 flash — erase via esp_partition API */
static loxboot_err_t esp32_flash_erase(void *ctx, uint32_t addr, size_t len)
{
    loxboot_esp32_flash_ctx_t *esp_ctx = (loxboot_esp32_flash_ctx_t *)ctx;

    if (esp_ctx == NULL || esp_ctx->partition == NULL || len == 0u) {
        return LOXBOOT_ERR_FLASH_ERASE;
    }

    esp_err_t status = esp_partition_erase_range(esp_ctx->partition, addr, len);
    if (status != ESP_OK) {
        return LOXBOOT_ERR_FLASH_ERASE;
    }

    return LOXBOOT_OK;
}

void loxboot_esp32_flash_adapter_init(
    loxboot_flash_adapter_t *out,
    loxboot_esp32_flash_ctx_t *ctx)
{
    out->ctx = ctx;
    out->read = esp32_flash_read;
    out->write = esp32_flash_write;
    out->erase = esp32_flash_erase;
}
