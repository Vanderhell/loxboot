#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "loxboot/loxboot.h"
#include "loxboot/loxboot_transport.h"

/* Internal state helpers from loxboot_core */
extern loxboot_err_t loxboot_state_read(loxboot_t *ctx, loxboot_state_t *out_state);

/* CRC16-CCITT table (poly 0x1021, init 0xFFFF) */
static const uint16_t g_crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

uint16_t loxboot_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0u; i < len; i++) {
        uint8_t index = (uint8_t)((crc >> 8) ^ data[i]);
        crc = (uint16_t)(((crc << 8) ^ g_crc16_table[index]) & 0xFFFFU);
    }
    return crc;
}

loxboot_err_t loxboot_uart_frame_encode(
    uint8_t cmd,
    const uint8_t *payload,
    uint16_t payload_len,
    uint8_t *out,
    size_t *out_len)
{
    if (out == NULL || out_len == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (payload_len > 0u && payload == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (payload_len > LOXBOOT_UART_MAX_FRAME_PAYLOAD) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    size_t frame_size = 1u + 1u + 1u + 1u + payload_len + 2u;
    if (*out_len < frame_size) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    size_t idx = 0u;
    out[idx++] = LOXBOOT_UART_SOF;
    out[idx++] = cmd;
    out[idx++] = (uint8_t)(payload_len & 0xFFU);
    out[idx++] = (uint8_t)((payload_len >> 8) & 0xFFU);

    if (payload != NULL && payload_len > 0u) {
        memcpy(&out[idx], payload, payload_len);
        idx += payload_len;
    }

    uint16_t crc = loxboot_crc16(&out[1], (size_t)(3u + payload_len));
    out[idx++] = (uint8_t)(crc & 0xFFU);
    out[idx++] = (uint8_t)((crc >> 8) & 0xFFU);

    *out_len = idx;
    return LOXBOOT_OK;
}

loxboot_err_t loxboot_uart_frame_decode(
    const uint8_t *in,
    size_t in_len,
    uint8_t *cmd_out,
    uint8_t *payload_out,
    uint16_t *payload_len_out)
{
    if (in == NULL || cmd_out == NULL || payload_len_out == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    if (in_len < 6u) {
        return LOXBOOT_ERR_TRANSPORT;
    }

    if (in[0] != LOXBOOT_UART_SOF) {
        return LOXBOOT_ERR_TRANSPORT;
    }

    *cmd_out = in[1];
    uint16_t payload_len = (uint16_t)(in[2] | (in[3] << 8));

    if (payload_len > LOXBOOT_UART_MAX_FRAME_PAYLOAD) {
        return LOXBOOT_ERR_TRANSPORT;
    }

    size_t frame_size = 1u + 1u + 1u + 1u + payload_len + 2u;
    if (in_len < frame_size) {
        return LOXBOOT_ERR_TRANSPORT;
    }

    uint16_t crc_received = (uint16_t)(in[frame_size - 2u] | (in[frame_size - 1u] << 8));
    uint16_t crc_computed = loxboot_crc16(&in[1], (size_t)(3u + payload_len));

    if (crc_received != crc_computed) {
        return LOXBOOT_ERR_TRANSPORT;
    }

    if (payload_len > 0u) {
        if (payload_out == NULL) {
            return LOXBOOT_ERR_INVALID_ARG;
        }
        memcpy(payload_out, &in[4], payload_len);
    }
    *payload_len_out = payload_len;

    return LOXBOOT_OK;
}

loxboot_err_t loxboot_uart_send_status(loxboot_uart_session_t *session)
{
    if (session == NULL || session->boot == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    uint8_t payload[4] = {
        session->boot->state.slots[0].state,
        session->boot->state.slots[1].state,
        session->boot->state.active_slot,
        session->boot->state.boot_reason
    };

    uint8_t frame[LOXBOOT_UART_MAX_FRAME_PAYLOAD + 8u];
    size_t frame_len = sizeof(frame);

    loxboot_err_t err = loxboot_uart_frame_encode(
        LOXBOOT_UART_RSP_STATUS,
        payload,
        sizeof(payload),
        frame,
        &frame_len);

    if (err != LOXBOOT_OK) {
        return err;
    }

    for (size_t i = 0u; i < frame_len; i++) {
        err = session->boot->transport.write_byte(session->boot->transport.ctx, frame[i]);
        if (err != LOXBOOT_OK) {
            return err;
        }
    }

    return session->boot->transport.flush(session->boot->transport.ctx);
}

loxboot_err_t loxboot_uart_run_session(loxboot_uart_session_t *session)
{
    if (session == NULL || session->boot == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    loxboot_t *ctx = session->boot;

    if (ctx->transport.read_byte == NULL || ctx->transport.write_byte == NULL || ctx->transport.flush == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }
    if (ctx->clock.now_ms == NULL) {
        return LOXBOOT_ERR_INVALID_ARG;
    }

    uint32_t listen_start = ctx->clock.now_ms(ctx->clock.ctx);
    uint32_t listen_timeout = (session->listen_ms > 0u) ? session->listen_ms : LOXBOOT_UART_LISTEN_MS;

    /* static: bootloader is single-threaded; avoids 1KB stack allocation on every call */
    static uint8_t payload_buf[LOXBOOT_UART_MAX_FRAME_PAYLOAD];
    loxboot_err_t err = LOXBOOT_OK;

    loxboot_state_t state;
    err = loxboot_state_read(ctx, &state);
    if (err != LOXBOOT_OK) {
        return err;
    }

    loxboot_slot_id_t active_slot = (loxboot_slot_id_t)state.active_slot;
    loxboot_slot_id_t target_slot = (active_slot == LOXBOOT_SLOT_A) ? LOXBOOT_SLOT_B : LOXBOOT_SLOT_A;
    uint32_t target_slot_base = (target_slot == LOXBOOT_SLOT_A) ?
                                ctx->platform.slot_a_base : ctx->platform.slot_b_base;

    session->_session_active = false;
    session->_bytes_written = 0u;
    session->_slot_erased = false;

    uint32_t frame_timeout = 5000u;

    while (1) {
        uint32_t now = ctx->clock.now_ms(ctx->clock.ctx);
        uint32_t elapsed = (uint32_t)(now - listen_start);

        if (elapsed >= listen_timeout) {
            return LOXBOOT_OK;
        }

        uint32_t remaining = (uint32_t)(listen_timeout - elapsed);
        uint8_t byte;
        err = ctx->transport.read_byte(ctx->transport.ctx, &byte, remaining);

        if (err == LOXBOOT_ERR_TIMEOUT) {
            return LOXBOOT_OK;
        }
        if (err != LOXBOOT_OK) {
            return err;
        }

        if (byte != LOXBOOT_UART_SOF) {
            continue;
        }

        size_t frame_idx = 0u;
        session->_frame_buf[frame_idx++] = byte;
        uint16_t payload_len = 0xFFFFU;

        while (frame_idx < sizeof(session->_frame_buf)) {
            err = ctx->transport.read_byte(ctx->transport.ctx, &session->_frame_buf[frame_idx], frame_timeout);
            if (err != LOXBOOT_OK) {
                loxboot_invalidate_slot(ctx, target_slot);
                uint8_t err_byte = (uint8_t)LOXBOOT_ERR_TRANSPORT;
                loxboot_uart_frame_encode(LOXBOOT_UART_RSP_ERROR, &err_byte, 1u, session->_frame_buf, &frame_idx);
                for (size_t i = 0u; i < frame_idx; i++) {
                    ctx->transport.write_byte(ctx->transport.ctx, session->_frame_buf[i]);
                }
                ctx->transport.flush(ctx->transport.ctx);
                break;
            }

            frame_idx++;

            if (frame_idx == 4u) {
                payload_len = (uint16_t)(session->_frame_buf[2] | (session->_frame_buf[3] << 8));
                if (payload_len > LOXBOOT_UART_MAX_FRAME_PAYLOAD) {
                    loxboot_invalidate_slot(ctx, target_slot);
                    uint8_t err_byte = (uint8_t)LOXBOOT_ERR_TRANSPORT;
                    size_t resp_len = sizeof(session->_frame_buf);
                    loxboot_uart_frame_encode(LOXBOOT_UART_RSP_ERROR, &err_byte, 1u, session->_frame_buf, &resp_len);
                    for (size_t i = 0u; i < resp_len; i++) {
                        ctx->transport.write_byte(ctx->transport.ctx, session->_frame_buf[i]);
                    }
                    ctx->transport.flush(ctx->transport.ctx);
                    break;
                }
            }

            if (frame_idx == 4u + payload_len + 2u) {
                break;
            }
        }

        if (err != LOXBOOT_OK) {
            continue;
        }

        uint8_t cmd;
        uint16_t pl_len;
        err = loxboot_uart_frame_decode(session->_frame_buf, frame_idx, &cmd, payload_buf, &pl_len);
        if (err != LOXBOOT_OK) {
            uint8_t err_byte = (uint8_t)LOXBOOT_ERR_TRANSPORT;
            size_t resp_len = sizeof(session->_frame_buf);
            loxboot_uart_frame_encode(LOXBOOT_UART_RSP_ERROR, &err_byte, 1u, session->_frame_buf, &resp_len);
            for (size_t i = 0u; i < resp_len; i++) {
                ctx->transport.write_byte(ctx->transport.ctx, session->_frame_buf[i]);
            }
            ctx->transport.flush(ctx->transport.ctx);
            continue;
        }

        uint8_t response_code = LOXBOOT_UART_RSP_OK;
        uint8_t response_payload[4u] = {0};
        uint16_t response_payload_len = 0u;

        switch (cmd) {
            case LOXBOOT_UART_CMD_HELLO: {
                session->_session_active = true;
                loxboot_state_read(ctx, &state);
                response_code = LOXBOOT_UART_RSP_STATUS;
                response_payload[0] = state.slots[0].state;
                response_payload[1] = state.slots[1].state;
                response_payload[2] = state.active_slot;
                response_payload[3] = state.boot_reason;
                response_payload_len = 4u;
                break;
            }

            case LOXBOOT_UART_CMD_WRITE: {
                if (!session->_session_active) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)LOXBOOT_ERR_INVALID_ARG;
                    response_payload_len = 1u;
                    break;
                }

                if (pl_len < 4u) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)LOXBOOT_ERR_INVALID_ARG;
                    response_payload_len = 1u;
                    break;
                }

                uint32_t offset = (uint32_t)(payload_buf[0] | (payload_buf[1] << 8) | (payload_buf[2] << 16) | (payload_buf[3] << 24));
                uint16_t write_len = (uint16_t)(pl_len - 4u);

                if (write_len > ctx->platform.slot_size ||
                    offset > (ctx->platform.slot_size - write_len)) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)LOXBOOT_ERR_INVALID_ARG;
                    response_payload_len = 1u;
                    break;
                }

                if (!session->_slot_erased) {
                    err = ctx->flash.erase(ctx->flash.ctx, target_slot_base, ctx->platform.slot_size);
                    if (err != LOXBOOT_OK) {
                        response_code = LOXBOOT_UART_RSP_ERROR;
                        response_payload[0] = (uint8_t)err;
                        response_payload_len = 1u;
                        loxboot_invalidate_slot(ctx, target_slot);
                        break;
                    }
                    session->_slot_erased = true;
                }

                err = ctx->flash.write(ctx->flash.ctx, target_slot_base + offset, &payload_buf[4], write_len);
                if (err != LOXBOOT_OK) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)err;
                    response_payload_len = 1u;
                    loxboot_invalidate_slot(ctx, target_slot);
                    break;
                }

                session->_bytes_written = offset + write_len;
                response_code = LOXBOOT_UART_RSP_OK;
                break;
            }

            case LOXBOOT_UART_CMD_COMMIT: {
                if (!session->_session_active) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)LOXBOOT_ERR_INVALID_ARG;
                    response_payload_len = 1u;
                    break;
                }

                if (pl_len < 8u) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)LOXBOOT_ERR_INVALID_ARG;
                    response_payload_len = 1u;
                    break;
                }

                uint32_t firmware_size = (uint32_t)(payload_buf[0] | (payload_buf[1] << 8) | (payload_buf[2] << 16) | (payload_buf[3] << 24));
                uint32_t firmware_crc32 = (uint32_t)(payload_buf[4] | (payload_buf[5] << 8) | (payload_buf[6] << 16) | (payload_buf[7] << 24));

                if (firmware_size != session->_bytes_written) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)LOXBOOT_ERR_INVALID_ARG;
                    response_payload_len = 1u;
                    loxboot_invalidate_slot(ctx, target_slot);
                    break;
                }

                err = loxboot_commit_slot(ctx, target_slot, firmware_size, firmware_crc32);
                if (err != LOXBOOT_OK) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)err;
                    response_payload_len = 1u;
                    loxboot_invalidate_slot(ctx, target_slot);
                    break;
                }

                /* Verify the written image matches the declared CRC before
                 * accepting the commit. loxboot_run() also checks this at boot,
                 * but "updater" platforms (e.g. ESP32 OTA) hand off without
                 * loxboot_run(), so a corrupt image must be caught here. */
                err = loxboot_verify_slot(ctx, target_slot);
                if (err != LOXBOOT_OK) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)err;
                    response_payload_len = 1u;
                    loxboot_invalidate_slot(ctx, target_slot);
                    break;
                }

                /* Record the committed slot so the caller (loxboot_run or an
                 * ESP32 update loop) can boot/handoff to it after the session. */
                session->_commit_done    = true;
                session->_committed_slot = target_slot;

                response_code = LOXBOOT_UART_RSP_OK;
                break;
            }

            case LOXBOOT_UART_CMD_ABORT: {
                loxboot_invalidate_slot(ctx, target_slot);
                response_code = LOXBOOT_UART_RSP_OK;
                session->_session_active = false;
                break;
            }

            case LOXBOOT_UART_CMD_STATUS: {
                err = loxboot_state_read(ctx, &state);
                if (err != LOXBOOT_OK) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)err;
                    response_payload_len = 1u;
                    break;
                }

                response_code = LOXBOOT_UART_RSP_STATUS;
                response_payload[0] = state.slots[0].state;
                response_payload[1] = state.slots[1].state;
                response_payload[2] = state.active_slot;
                response_payload[3] = state.boot_reason;
                response_payload_len = 4u;
                break;
            }

            case LOXBOOT_UART_CMD_REBOOT: {
                if (!session->_session_active) {
                    response_code = LOXBOOT_UART_RSP_ERROR;
                    response_payload[0] = (uint8_t)LOXBOOT_ERR_INVALID_ARG;
                    response_payload_len = 1u;
                    break;
                }

                session->_reboot_requested = true;

                size_t resp_len = sizeof(session->_frame_buf);
                loxboot_uart_frame_encode(LOXBOOT_UART_RSP_OK, NULL, 0u, session->_frame_buf, &resp_len);
                for (size_t i = 0u; i < resp_len; i++) {
                    ctx->transport.write_byte(ctx->transport.ctx, session->_frame_buf[i]);
                }
                ctx->transport.flush(ctx->transport.ctx);
                return LOXBOOT_OK;
            }

            default: {
                response_code = LOXBOOT_UART_RSP_ERROR;
                response_payload[0] = (uint8_t)LOXBOOT_ERR_INVALID_ARG;
                response_payload_len = 1u;
                break;
            }
        }

        size_t resp_len = sizeof(session->_frame_buf);
        loxboot_uart_frame_encode(response_code, response_payload, response_payload_len, session->_frame_buf, &resp_len);
        for (size_t i = 0u; i < resp_len; i++) {
            err = ctx->transport.write_byte(ctx->transport.ctx, session->_frame_buf[i]);
            if (err != LOXBOOT_OK) {
                return err;
            }
        }
        err = ctx->transport.flush(ctx->transport.ctx);
        if (err != LOXBOOT_OK) {
            return err;
        }
    }

    return LOXBOOT_OK;
}
