#ifndef LOXBOOT_TEST_SUPPORT_H
#define LOXBOOT_TEST_SUPPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "loxboot/loxboot.h"

#define TEST_FLASH_SIZE (128u * 1024u)

typedef struct {
    uint8_t mem[TEST_FLASH_SIZE];
    bool fail_next_read;
    bool fail_next_write;
    bool fail_next_erase;
    bool fail_erase_on_addr;
    bool fail_write_on_addr;
    bool fail_read_on_addr;
    uint32_t fail_addr;
} test_flash_t;

typedef struct {
    int count;
    loxboot_err_t last_reason;
} test_fatal_t;

void test_flash_reset(test_flash_t *flash);

loxboot_err_t test_flash_read(void *ctx, uint32_t addr, uint8_t *buf, size_t len);
loxboot_err_t test_flash_write(void *ctx, uint32_t addr, const uint8_t *buf, size_t len);
loxboot_err_t test_flash_erase(void *ctx, uint32_t addr, size_t len);

void test_on_fatal(void *ctx, loxboot_err_t reason);

void test_make_valid_ctx(loxboot_t *ctx, test_flash_t *flash, test_fatal_t *fatal);
void test_seed_state(test_flash_t *flash, const loxboot_platform_t *platform, const loxboot_state_t *state);
void test_build_default_state(loxboot_state_t *out, loxboot_slot_id_t active_slot);

void test_read_state_copy(const test_flash_t *flash, uint32_t base, loxboot_state_t *out_state);

extern int g_test_passed;
extern int g_test_failed;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            g_test_failed++; \
            (void)printf("CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            return; \
        } \
        g_test_passed++; \
    } while (0)

#define CHECK_EQ_U32(a, b) \
    do { \
        uint32_t _a = (uint32_t)(a); \
        uint32_t _b = (uint32_t)(b); \
        if (_a != _b) { \
            g_test_failed++; \
            (void)printf("CHECK_EQ_U32 failed: %s=%lu %s=%lu (%s:%d)\n", \
                         #a, (unsigned long)_a, #b, (unsigned long)_b, __FILE__, __LINE__); \
            return; \
        } \
        g_test_passed++; \
    } while (0)

#define CHECK_EQ_INT(a, b) \
    do { \
        int _a = (int)(a); \
        int _b = (int)(b); \
        if (_a != _b) { \
            g_test_failed++; \
            (void)printf("CHECK_EQ_INT failed: %s=%d %s=%d (%s:%d)\n", #a, _a, #b, _b, __FILE__, __LINE__); \
            return; \
        } \
        g_test_passed++; \
    } while (0)

typedef void (*test_fn_t)(void);

void run_test(const char *name, test_fn_t fn);

#endif /* LOXBOOT_TEST_SUPPORT_H */
