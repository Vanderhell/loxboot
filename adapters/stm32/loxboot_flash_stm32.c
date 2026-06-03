#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "loxboot/loxboot.h"
#include "stm32_hal.h"

/* STM32 internal flash — memory-mapped read */
static loxboot_err_t stm32_flash_read(void *ctx, uint32_t addr, uint8_t *buf, size_t len)
{
    (void)ctx;

    if (buf == NULL) {
        return LOXBOOT_ERR_FLASH_READ;
    }

    /* Internal flash is memory-mapped — direct pointer read */
    const uint8_t *src = (const uint8_t *)addr;
    memcpy(buf, src, len);
    return LOXBOOT_OK;
}

/* STM32 internal flash — write via HAL */
static loxboot_err_t stm32_flash_write(void *ctx, uint32_t addr, const uint8_t *buf, size_t len)
{
    (void)ctx;

    if (buf == NULL || len == 0u) {
        return LOXBOOT_ERR_FLASH_WRITE;
    }

    HAL_FLASH_Unlock();

    /* Write in 8-byte chunks (FLASH_TYPEPROGRAM_DOUBLEWORD) */
    size_t written = 0u;
    while (written < len) {
        size_t chunk = (len - written > 8u) ? 8u : (len - written);

        /* Align partial final chunk with zeros */
        uint64_t data = 0u;
        memcpy(&data, &buf[written], chunk);

        HAL_StatusTypeDef status = HAL_FLASH_Program(
            FLASH_TYPEPROGRAM_DOUBLEWORD,
            addr + written,
            data);

        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return LOXBOOT_ERR_FLASH_WRITE;
        }

        written += chunk;
    }

    HAL_FLASH_Lock();
    return LOXBOOT_OK;
}

/* STM32 internal flash — erase via HAL */
static loxboot_err_t stm32_flash_erase(void *ctx, uint32_t addr, size_t len)
{
    (void)ctx;

    if (len == 0u) {
        return LOXBOOT_OK;
    }

    /* Check alignment (erase must be page-aligned) */
    if ((addr % FLASH_PAGE_SIZE) != 0u || (len % FLASH_PAGE_SIZE) != 0u) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Banks = FLASH_BANK_1;
    erase_init.Page = addr / FLASH_PAGE_SIZE;
    erase_init.NbPages = len / FLASH_PAGE_SIZE;

    uint32_t page_error = 0u;

    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    HAL_FLASH_Lock();

    if (status != HAL_OK) {
        return LOXBOOT_ERR_FLASH_ERASE;
    }

    return LOXBOOT_OK;
}

void loxboot_stm32_flash_adapter_init(
    loxboot_flash_adapter_t *out,
    loxboot_stm32_flash_ctx_t *ctx)
{
    out->ctx = ctx;
    out->read = stm32_flash_read;
    out->write = stm32_flash_write;
    out->erase = stm32_flash_erase;
}
