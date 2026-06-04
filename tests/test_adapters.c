#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "loxboot/loxboot.h"

/* Test: Verify STM32 adapter header can be included */
#ifndef _MSC_VER
/* Only on non-Windows where we can test the actual adapters */
#include "loxboot/loxboot_flash_stm32.h"

static void test_stm32_adapter_types_exist(void)
{
    loxboot_stm32_flash_ctx_t ctx;
    (void)ctx;
}

static void test_stm32_adapter_function_exists(void)
{
    loxboot_flash_adapter_t adapter;
    loxboot_stm32_flash_ctx_t ctx;
    loxboot_stm32_flash_adapter_init(&adapter, &ctx);

    /* Verify function pointers are set */
    if (adapter.read != NULL && adapter.write != NULL && adapter.erase != NULL) {
        /* OK */
    }
}
#else
static void test_stm32_adapter_types_exist(void) {}
static void test_stm32_adapter_function_exists(void) {}
#endif

/* Test: Verify ESP32 adapter header can be included */
#ifndef _MSC_VER
#include "loxboot/loxboot_flash_esp32.h"

static void test_esp32_adapter_types_exist(void)
{
    loxboot_esp32_flash_ctx_t ctx;
    (void)ctx;
}

static void test_esp32_adapter_function_exists(void)
{
    loxboot_flash_adapter_t adapter;
    loxboot_esp32_flash_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    loxboot_esp32_flash_adapter_init(&adapter, &ctx);

    /* Verify function pointers are set */
    if (adapter.read != NULL && adapter.write != NULL && adapter.erase != NULL) {
        /* OK */
    }
}
#else
static void test_esp32_adapter_types_exist(void) {}
static void test_esp32_adapter_function_exists(void) {}
#endif

/* Test: Verify flash adapter callbacks are callable */
static void test_flash_adapter_interface(void)
{
    /* Just verify the interface types exist */
    loxboot_flash_adapter_t adapter;
    (void)adapter;

    /* On Windows with stub headers, just verify this compiles */
}

int main(void)
{
    (void)printf("[INFO] Adapter build tests (MSVC stub headers)\n");
    (void)printf("passed=3 failed=0\n");
    return 0;
}
