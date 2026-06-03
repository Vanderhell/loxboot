#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"
#include "test_support.h"

/* Test CRC16-CCITT known vectors */
static void test_crc16_known_vectors(void)
{
    uint8_t data1[] = {0x01};
    uint8_t data2[] = {0x01, 0x00, 0x00};

    uint16_t crc1 = loxboot_crc16(data1, sizeof(data1));
    uint16_t crc2 = loxboot_crc16(data2, sizeof(data2));

    CHECK(crc1 != 0u);
    CHECK(crc2 != 0u);
    CHECK(crc1 != crc2);
}

/* Test CRC16 with empty input */
static void test_crc16_empty(void)
{
    uint16_t crc = loxboot_crc16(NULL, 0u);
    CHECK_EQ_INT(crc, 0xFFFF);
}

/* Test CRC16 with known test vector (CCITT-FALSE standard) */
static void test_crc16_known_vector(void)
{
    /* "123456789" should produce 0x29B1 for CRC16-CCITT-FALSE */
    uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint16_t crc = loxboot_crc16(data, sizeof(data));

    /* Known good value for CRC16-CCITT-FALSE (poly 0x1021, init 0xFFFF) */
    CHECK_EQ_INT(crc, 0x29B1);
}

/* Test CRC16 consistency */
static void test_crc16_consistency(void)
{
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint16_t crc1 = loxboot_crc16(data, sizeof(data));
    uint16_t crc2 = loxboot_crc16(data, sizeof(data));
    CHECK_EQ_INT(crc1, crc2);
}

/* Test CRC16 changes with data modification */
static void test_crc16_data_sensitivity(void)
{
    uint8_t data1[] = {0xAA, 0xBB, 0xCC};
    uint8_t data2[] = {0xAA, 0xBB, 0xDD};

    uint16_t crc1 = loxboot_crc16(data1, sizeof(data1));
    uint16_t crc2 = loxboot_crc16(data2, sizeof(data2));

    CHECK(crc1 != crc2);
}

int main(void)
{
    run_test("crc16_known_vectors", test_crc16_known_vectors);
    run_test("crc16_empty", test_crc16_empty);
    run_test("crc16_known_vector", test_crc16_known_vector);
    run_test("crc16_consistency", test_crc16_consistency);
    run_test("crc16_data_sensitivity", test_crc16_data_sensitivity);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
