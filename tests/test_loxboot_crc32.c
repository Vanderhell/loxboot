#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "test_support.h"

static void test_empty(void)
{
    CHECK_EQ_U32(loxboot_crc32(NULL, 0), 0x00000000u);
    CHECK_EQ_U32(loxboot_crc32((const uint8_t *)"", 0), 0x00000000u);
}

static void test_known_vector(void)
{
    const char *s = "123456789";
    uint32_t crc = loxboot_crc32((const uint8_t *)s, strlen(s));
    CHECK_EQ_U32(crc, 0xCBF43926u);
}

static void test_binary_buffer(void)
{
    const uint8_t buf[] = { 0x00u, 0x01u, 0x02u, 0x10u, 0xFFu, 0x5Au, 0xA5u };
    uint32_t crc1 = loxboot_crc32(buf, sizeof(buf));
    uint32_t crc2 = loxboot_crc32(buf, sizeof(buf));
    CHECK(crc1 != 0u);
    CHECK_EQ_U32(crc1, crc2);
}

static void test_null_nonzero_len(void)
{
    CHECK_EQ_U32(loxboot_crc32(NULL, 1), 0xFFFFFFFFu);
    CHECK_EQ_U32(loxboot_crc32(NULL, 16), 0xFFFFFFFFu);
}

int main(void)
{
    run_test("crc32/empty", test_empty);
    run_test("crc32/known_vector", test_known_vector);
    run_test("crc32/binary", test_binary_buffer);
    run_test("crc32/null_nonzero", test_null_nonzero_len);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}

