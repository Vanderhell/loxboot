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

/* Test frame encode/decode roundtrip with payload */
static void test_frame_roundtrip_with_payload(void)
{
    uint8_t payload_in[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t frame_buf[256];
    size_t frame_len = sizeof(frame_buf);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_WRITE,
        payload_in,
        sizeof(payload_in),
        frame_buf,
        &frame_len);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK(frame_len > 0u && frame_len <= sizeof(frame_buf));

    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    err = loxboot_uart_frame_decode(
        frame_buf,
        frame_len,
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(cmd_out, LOXBOOT_UART_CMD_WRITE);
    CHECK_EQ_INT(payload_len_out, sizeof(payload_in));
    CHECK(!memcmp(payload_out, payload_in, payload_len_out));
}

/* Test frame encode/decode roundtrip with zero payload */
static void test_frame_roundtrip_zero_payload(void)
{
    uint8_t frame_buf[256];
    size_t frame_len = sizeof(frame_buf);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_HELLO,
        NULL,
        0u,
        frame_buf,
        &frame_len);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(frame_len, 6u);

    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    err = loxboot_uart_frame_decode(
        frame_buf,
        frame_len,
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(cmd_out, LOXBOOT_UART_CMD_HELLO);
    CHECK_EQ_INT(payload_len_out, 0u);
}

/* Test frame encode NULL validation */
static void test_frame_encode_null_out(void)
{
    uint8_t payload[] = {0x01};
    size_t out_len = 256u;

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_HELLO,
        payload,
        1u,
        NULL,
        &out_len);
    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_ARG);
}

/* Test frame encode NULL out_len */
static void test_frame_encode_null_out_len(void)
{
    uint8_t payload[] = {0x01};
    uint8_t frame_buf[256];

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_HELLO,
        payload,
        1u,
        frame_buf,
        NULL);
    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_ARG);
}

/* Test frame encode payload without pointer */
static void test_frame_encode_payload_null_ptr(void)
{
    uint8_t frame_buf[256];
    size_t frame_len = sizeof(frame_buf);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_WRITE,
        NULL,
        4u,
        frame_buf,
        &frame_len);
    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_ARG);
}

/* Test frame decode NULL input */
static void test_frame_decode_null_input(void)
{
    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    loxboot_err_t err = loxboot_uart_frame_decode(
        NULL,
        10u,
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_ARG);
}

/* Test frame decode NULL cmd_out */
static void test_frame_decode_null_cmd_out(void)
{
    uint8_t frame[] = {0x7E, 0x01, 0x00, 0x00, 0x00, 0x00};
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    loxboot_err_t err = loxboot_uart_frame_decode(
        frame,
        sizeof(frame),
        NULL,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_ARG);
}

/* Test frame decode NULL payload_len_out */
static void test_frame_decode_null_payload_len_out(void)
{
    uint8_t frame[] = {0x7E, 0x01, 0x00, 0x00, 0x00, 0x00};
    uint8_t cmd_out;
    uint8_t payload_out[256];

    loxboot_err_t err = loxboot_uart_frame_decode(
        frame,
        sizeof(frame),
        &cmd_out,
        payload_out,
        NULL);
    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_ARG);
}

/* Test frame decode with payload but NULL payload_out */
static void test_frame_decode_payload_null_ptr(void)
{
    uint8_t payload_in[] = {0x01, 0x02};
    uint8_t frame_buf[256];
    size_t frame_len = sizeof(frame_buf);

    loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_WRITE,
        payload_in,
        sizeof(payload_in),
        frame_buf,
        &frame_len);

    uint8_t cmd_out;
    uint16_t payload_len_out;

    loxboot_err_t err = loxboot_uart_frame_decode(
        frame_buf,
        frame_len,
        &cmd_out,
        NULL,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_ARG);
}

/* Test frame decode with short frame */
static void test_frame_decode_short_frame(void)
{
    uint8_t frame[] = {0x7E, 0x01, 0x00};
    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    loxboot_err_t err = loxboot_uart_frame_decode(
        frame,
        sizeof(frame),
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_ERR_TRANSPORT);
}

/* Test frame decode with bad SOF */
static void test_frame_decode_bad_sof(void)
{
    uint8_t frame[] = {0xFF, 0x01, 0x00, 0x00, 0x00, 0x00};
    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    loxboot_err_t err = loxboot_uart_frame_decode(
        frame,
        sizeof(frame),
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_ERR_TRANSPORT);
}

/* Test frame decode with bad CRC */
static void test_frame_decode_bad_crc(void)
{
    uint8_t frame[] = {0x7E, 0x01, 0x00, 0x00, 0xFF, 0xFF};
    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    loxboot_err_t err = loxboot_uart_frame_decode(
        frame,
        sizeof(frame),
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_ERR_TRANSPORT);
}

/* Test encode/decode STATUS with zero payload */
static void test_frame_status_zero_payload(void)
{
    uint8_t frame_buf[256];
    size_t frame_len = sizeof(frame_buf);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_RSP_STATUS,
        NULL,
        0u,
        frame_buf,
        &frame_len);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(frame_len, 6u);

    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    err = loxboot_uart_frame_decode(
        frame_buf,
        frame_len,
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(cmd_out, LOXBOOT_UART_RSP_STATUS);
    CHECK_EQ_INT(payload_len_out, 0u);
}

/* Test encode/decode ABORT with zero payload */
static void test_frame_abort_zero_payload(void)
{
    uint8_t frame_buf[256];
    size_t frame_len = sizeof(frame_buf);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_ABORT,
        NULL,
        0u,
        frame_buf,
        &frame_len);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    err = loxboot_uart_frame_decode(
        frame_buf,
        frame_len,
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(cmd_out, LOXBOOT_UART_CMD_ABORT);
    CHECK_EQ_INT(payload_len_out, 0u);
}

/* Test encode/decode REBOOT with zero payload */
static void test_frame_reboot_zero_payload(void)
{
    uint8_t frame_buf[256];
    size_t frame_len = sizeof(frame_buf);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_REBOOT,
        NULL,
        0u,
        frame_buf,
        &frame_len);
    CHECK_EQ_INT(err, LOXBOOT_OK);

    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    err = loxboot_uart_frame_decode(
        frame_buf,
        frame_len,
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_OK);
    CHECK_EQ_INT(cmd_out, LOXBOOT_UART_CMD_REBOOT);
    CHECK_EQ_INT(payload_len_out, 0u);
}

/* Test frame encode with oversized payload */
static void test_frame_encode_payload_too_large(void)
{
    uint8_t huge_payload[2048] = {0};
    uint8_t frame_buf[256];
    size_t frame_len = sizeof(frame_buf);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_CMD_WRITE,
        huge_payload,
        sizeof(huge_payload),
        frame_buf,
        &frame_len);
    CHECK_EQ_INT(err, LOXBOOT_ERR_INVALID_ARG);
}

/* Test frame decode with oversized payload_len field */
static void test_frame_decode_payload_len_too_large(void)
{
    uint8_t frame[] = {
        0x7E,           /* SOF */
        0x02,           /* CMD_WRITE */
        0x00, 0x10,     /* payload_len = 0x1000 (too large) */
        0x00, 0x00
    };
    uint8_t cmd_out;
    uint8_t payload_out[256];
    uint16_t payload_len_out;

    loxboot_err_t err = loxboot_uart_frame_decode(
        frame,
        sizeof(frame),
        &cmd_out,
        payload_out,
        &payload_len_out);
    CHECK_EQ_INT(err, LOXBOOT_ERR_TRANSPORT);
}

int main(void)
{
    run_test("crc16_known_vectors", test_crc16_known_vectors);
    run_test("crc16_empty", test_crc16_empty);
    run_test("crc16_known_vector", test_crc16_known_vector);
    run_test("crc16_consistency", test_crc16_consistency);
    run_test("crc16_data_sensitivity", test_crc16_data_sensitivity);

    run_test("frame_roundtrip_with_payload", test_frame_roundtrip_with_payload);
    run_test("frame_roundtrip_zero_payload", test_frame_roundtrip_zero_payload);

    run_test("frame_encode_null_out", test_frame_encode_null_out);
    run_test("frame_encode_null_out_len", test_frame_encode_null_out_len);
    run_test("frame_encode_payload_null_ptr", test_frame_encode_payload_null_ptr);

    run_test("frame_decode_null_input", test_frame_decode_null_input);
    run_test("frame_decode_null_cmd_out", test_frame_decode_null_cmd_out);
    run_test("frame_decode_null_payload_len_out", test_frame_decode_null_payload_len_out);
    run_test("frame_decode_payload_null_ptr", test_frame_decode_payload_null_ptr);

    run_test("frame_decode_short_frame", test_frame_decode_short_frame);
    run_test("frame_decode_bad_sof", test_frame_decode_bad_sof);
    run_test("frame_decode_bad_crc", test_frame_decode_bad_crc);

    run_test("frame_status_zero_payload", test_frame_status_zero_payload);
    run_test("frame_abort_zero_payload", test_frame_abort_zero_payload);
    run_test("frame_reboot_zero_payload", test_frame_reboot_zero_payload);
    run_test("frame_encode_payload_too_large", test_frame_encode_payload_too_large);
    run_test("frame_decode_payload_len_too_large", test_frame_decode_payload_len_too_large);

    (void)printf("passed=%d failed=%d\n", g_test_passed, g_test_failed);
    return (g_test_failed > 0) ? 1 : 0;
}
