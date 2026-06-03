#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"
#include "test_support.h"

/* Test CRC16-CCITT with known vectors */
static void test_crc16_known_vectors(void)
{
    uint8_t data1[] = {0x01};
    uint8_t data2[] = {0x01, 0x00, 0x00};
    uint8_t data3[] = {0x01, 0x00, 0x00, 0xAA, 0xAA};

    uint16_t crc1 = loxboot_crc16(data1, sizeof(data1));
    uint16_t crc2 = loxboot_crc16(data2, sizeof(data2));
    uint16_t crc3 = loxboot_crc16(data3, sizeof(data3));

    CHECK(crc1 != 0u);
    CHECK(crc2 != 0u);
    CHECK(crc3 != 0u);
    CHECK(crc1 != crc2);
}

/* Test CRC16 with empty input */
static void test_crc16_empty(void)
{
    uint16_t crc = loxboot_crc16(NULL, 0u);
    CHECK(crc == 0xFFFFU);
}

/* Test frame encode with valid payload */
static void test_frame_encode_valid(void)
{
    uint8_t payload[] = {0x01, 0x02, 0x03};
    uint8_t frame[256];
    size_t frame_len = sizeof(frame);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_HELLO,
        payload,
        sizeof(payload),
        frame,
        &frame_len);

    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK(frame_len >= 7u);
    CHECK_EQ_INT(frame[0], LOXBOOT_UART_SOF);
    CHECK_EQ_INT(frame[1], LOXBOOT_UART_CMD_HELLO);
    CHECK_EQ_INT(frame[2], 3u);
    CHECK_EQ_INT(frame[3], 0u);
}

/* Test frame encode with no payload */
static void test_frame_encode_empty_payload(void)
{
    uint8_t frame[256];
    size_t frame_len = sizeof(frame);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_STATUS,
        NULL,
        0u,
        frame,
        &frame_len);

    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(frame_len, 7u);
    CHECK_EQ_INT(frame[0], LOXBOOT_UART_SOF);
    CHECK_EQ_INT(frame[1], LOXBOOT_UART_CMD_STATUS);
    CHECK_EQ_INT(frame[2], 0u);
    CHECK_EQ_INT(frame[3], 0u);
}

/* Test frame encode with max payload */
static void test_frame_encode_max_payload(void)
{
    uint8_t payload[LOXBOOT_UART_MAX_FRAME_PAYLOAD];
    memset(payload, 0xAA, sizeof(payload));

    uint8_t frame[LOXBOOT_UART_MAX_FRAME_PAYLOAD + 7u];
    size_t frame_len = sizeof(frame);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_WRITE,
        payload,
        sizeof(payload),
        frame,
        &frame_len);

    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(frame_len, LOXBOOT_UART_MAX_FRAME_PAYLOAD + 7u);
}

/* Test frame decode with valid frame */
static void test_frame_decode_valid(void)
{
    uint8_t payload_send[] = {0x01, 0x02, 0x03};
    uint8_t frame[256];
    size_t frame_len = sizeof(frame);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_HELLO,
        payload_send,
        sizeof(payload_send),
        frame,
        &frame_len);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    uint8_t cmd;
    uint8_t payload_recv[256];
    uint16_t payload_len;

    err = loxboot_uart_frame_decode(frame, frame_len, &cmd, payload_recv, &payload_len);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(cmd, LOXBOOT_UART_CMD_HELLO);
    CHECK_EQ_INT(payload_len, 3u);
    CHECK_EQ_INT(payload_recv[0], 0x01);
    CHECK_EQ_INT(payload_recv[1], 0x02);
    CHECK_EQ_INT(payload_recv[2], 0x03);
}

/* Test frame decode with corrupted CRC */
static void test_frame_decode_crc_fail(void)
{
    uint8_t payload_send[] = {0x01, 0x02, 0x03};
    uint8_t frame[256];
    size_t frame_len = sizeof(frame);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_HELLO,
        payload_send,
        sizeof(payload_send),
        frame,
        &frame_len);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    frame[frame_len - 1u] ^= 0xFFu;

    uint8_t cmd;
    uint8_t payload_recv[256];
    uint16_t payload_len;

    err = loxboot_uart_frame_decode(frame, frame_len, &cmd, payload_recv, &payload_len);
    CHECK_EQ_INT(err, LOXBOOT_ERR_TRANSPORT);
}

/* Test frame decode with missing SOF */
static void test_frame_decode_no_sof(void)
{
    uint8_t frame[7] = {0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint8_t cmd;
    uint8_t payload[256];
    uint16_t payload_len;

    loxboot_err_t err = loxboot_uart_frame_decode(frame, sizeof(frame), &cmd, payload, &payload_len);
    CHECK_EQ_INT(err, LOXBOOT_ERR_TRANSPORT);
}

/* Test frame decode with incomplete frame */
static void test_frame_decode_incomplete(void)
{
    uint8_t frame[3] = {LOXBOOT_UART_SOF, 0x01, 0x00};

    uint8_t cmd;
    uint8_t payload[256];
    uint16_t payload_len;

    loxboot_err_t err = loxboot_uart_frame_decode(frame, sizeof(frame), &cmd, payload, &payload_len);
    CHECK_EQ_INT(err, LOXBOOT_ERR_TRANSPORT);
}

int main(void)
{
    run_test("crc16_known_vectors", test_crc16_known_vectors);
    run_test("crc16_empty", test_crc16_empty);
    run_test("frame_encode_valid", test_frame_encode_valid);
    run_test("frame_encode_empty_payload", test_frame_encode_empty_payload);
    run_test("frame_encode_max_payload", test_frame_encode_max_payload);
    run_test("frame_decode_valid", test_frame_decode_valid);
    run_test("frame_decode_crc_fail", test_frame_decode_crc_fail);
    run_test("frame_decode_no_sof", test_frame_decode_no_sof);
    run_test("frame_decode_incomplete", test_frame_decode_incomplete);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
